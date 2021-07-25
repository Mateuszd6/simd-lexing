// TODO: Test both comments and a string at the end of a file (second parse)

extern char const* __asan_default_options(void);
extern char const* __asan_default_options() { return "detect_leaks=0"; }

#include <stdio.h>
#include <string.h>

#include <pmmintrin.h>
#include <immintrin.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "util.h"

#ifdef RELEASE
#  undef ASSERT
#  define ASSERT(...)
#  define printf(...) ((void) 0)
#  define NOOPTIMIZE(EXPR) asm volatile (""::"g"(&EXPR):"memory");
#  define PRINT_LINES 0
#else
#  define NOOPTIMIZE(EXPR) ((void) 0)
#  define PRINT_LINES 1
#endif

#define TOK2(X, Y) (((u32) X) | ((u32) Y) << 8)
#define TOK3(X, Y, Z) (((u32) X) | ((u32) Y) << 8 | ((u32) Z) << 16)

typedef __m256i lexbuf;
static const int nlex = sizeof(lexbuf);

enum carry
{
    CARRY_NONE = 0,
    CARRY_IDENT = 1,
};

enum lex_in
{
    IN_NONE = 0,
    IN_SHORT_COMMENT = 1,
    IN_LONG_COMMENT = 2,
    IN_STRING = 3,
};

static inline __m256i
mm_ext_shl8_si256(__m256i a)
{
    /* TODO: This is probably slow, figure out better way! */
    u64 carry = (u64) ((u8) _mm256_extract_epi8(a, 15));
    __m256i shifted = _mm256_bslli_epi128(a, 1);
    __m256i andcarry = _mm256_set_epi64x(0, carry, 0, 0);
    return _mm256_or_si256(shifted, andcarry);
}

static inline u32
u32_loadu(void* p)
{
    u32 retval;
    memcpy(&retval, p, sizeof retval);

    return retval;
}

// TODO: Get rid of them?
#if 1
static i64 n_parsed_idents = 0;
static i64 n_parsed_numbers = 0;
static i64 n_parsed_strings = 0;
static i64 n_parsed_chars = 0;
static i64 n_single_comments = 0;
static i64 n_long_comments = 0;
static i64 n_single_comments_skipped = 0;
static i64 n_long_comments_skipped = 0;
static char* g_fname = 0;
#endif

typedef struct lex_state lex_state;
struct lex_state
{
    char* string;
    char* string_end;
    i64 curr_line;
    i64 curr_inline_idx;
    i32 carry;
    i32 in;
};

static char*
lex(lex_state* state)
{
    char* p = state->string;
    char* string_end = state->string_end;
    i64 curr_line = state->curr_line;
    i64 curr_inline_idx = state->curr_inline_idx;
    i32 carry = state->carry;

    lexbuf cmpmask_0 = _mm256_set1_epi8('0' - 1);
    lexbuf cmpmask_9 = _mm256_set1_epi8('9' + 1);
    lexbuf cmpmask_a = _mm256_set1_epi8('a' - 1);
    lexbuf cmpmask_z = _mm256_set1_epi8('z' + 1);
    lexbuf cmpmask_A = _mm256_set1_epi8('A' - 1);
    lexbuf cmpmask_Z = _mm256_set1_epi8('Z' + 1);
    lexbuf cmpmask_underscore = _mm256_set1_epi8('_');
    lexbuf cmpmask_newline = _mm256_set1_epi8('\n');
    lexbuf cmpmask_doublequote = _mm256_set1_epi8('\"');
    lexbuf cmpmask_char_start = _mm256_set1_epi8(0x20); // space - 1, TODO: For sure?

    if (UNLIKELY(state->in != IN_NONE))
    {
        switch (state->in) {
        case IN_STRING: goto in_string;
        case IN_SHORT_COMMENT: goto in_short_comment;
            // case IN_LONG_COMMENT: goto
        default: NOTREACHED;
        }
    }

    while (p < string_end)
    {
        lexbuf b_1 = _mm256_loadu_si256((void*) p);
        lexbuf b_2 = _mm256_loadu_si256((void*) (p + sizeof(lexbuf)));

        lexbuf charmask_1 = _mm256_cmpgt_epi8(b_1, cmpmask_char_start);
        lexbuf charmask_2 = _mm256_cmpgt_epi8(b_2, cmpmask_char_start);

        lexbuf mask1_1 = _mm256_and_si256(_mm256_cmpgt_epi8(b_1, cmpmask_0), _mm256_cmpgt_epi8(cmpmask_9, b_1));
        lexbuf mask2_1 = _mm256_and_si256(_mm256_cmpgt_epi8(b_1, cmpmask_a), _mm256_cmpgt_epi8(cmpmask_z, b_1));
        lexbuf mask3_1 = _mm256_and_si256(_mm256_cmpgt_epi8(b_1, cmpmask_A), _mm256_cmpgt_epi8(cmpmask_Z, b_1));
        lexbuf mask4_1 = _mm256_cmpeq_epi8(b_1, cmpmask_underscore);

        lexbuf mask1_2 = _mm256_and_si256(_mm256_cmpgt_epi8(b_2, cmpmask_0), _mm256_cmpgt_epi8(cmpmask_9, b_2));
        lexbuf mask2_2 = _mm256_and_si256(_mm256_cmpgt_epi8(b_2, cmpmask_a), _mm256_cmpgt_epi8(cmpmask_z, b_2));
        lexbuf mask3_2 = _mm256_and_si256(_mm256_cmpgt_epi8(b_2, cmpmask_A), _mm256_cmpgt_epi8(cmpmask_Z, b_2));
        lexbuf mask4_2 = _mm256_cmpeq_epi8(b_2, cmpmask_underscore);

        lexbuf idents_mask_1 = _mm256_or_si256(_mm256_or_si256(mask1_1, mask2_1), _mm256_or_si256(mask3_1, mask4_1));
        lexbuf idents_mask_2 = _mm256_or_si256(_mm256_or_si256(mask1_2, mask2_2), _mm256_or_si256(mask3_2, mask4_2));

        // TODO: Try not to shift it
        lexbuf idents_mask_shed_1 = mm_ext_shl8_si256(idents_mask_1);
        lexbuf idents_mask_shed_2 = mm_ext_shl8_si256(idents_mask_2);

        lexbuf toks_mask_1 = _mm256_andnot_si256(idents_mask_1, charmask_1);
        lexbuf toks_mask_2 = _mm256_andnot_si256(idents_mask_2, charmask_2);

        lexbuf ident_start = _mm256_set1_epi16(0xFF00);
        lexbuf idents_startmask1_1 = _mm256_cmpeq_epi16(idents_mask_1, ident_start);
        lexbuf idents_startmask2_1 = _mm256_cmpeq_epi16(idents_mask_shed_1, ident_start);
        lexbuf idents_startmask1_2 = _mm256_cmpeq_epi16(idents_mask_2, ident_start);
        lexbuf idents_startmask2_2 = _mm256_cmpeq_epi16(idents_mask_shed_2, ident_start);

        lexbuf nidents_mask1_1 = _mm256_and_si256(idents_mask_1, idents_startmask1_1);
        lexbuf nidents_mask2_1 = _mm256_and_si256(idents_mask_shed_1, idents_startmask2_1);
        lexbuf nidents_mask1_2 = _mm256_and_si256(idents_mask_2, idents_startmask1_2);
        lexbuf nidents_mask2_2 = _mm256_and_si256(idents_mask_shed_2, idents_startmask2_2);

#if 1 // TODO : if it is slower, don't do it?
        // TODO: Maybe do these only once?
        lexbuf b1_shed = mm_ext_shl8_si256(b_1);
        lexbuf b2_shed = mm_ext_shl8_si256(b_2);
        lexbuf long_comment_end = _mm256_set1_epi16((u16) TOK2('*', '/'));

        lexbuf long_comment_end_mask1_1 = _mm256_cmpeq_epi16(b_1, long_comment_end);
        lexbuf long_comment_end_mask2_1 = _mm256_cmpeq_epi16(b1_shed, long_comment_end);
        lexbuf long_comment_end_mask1_2 = _mm256_cmpeq_epi16(b_2, long_comment_end);
        lexbuf long_comment_end_mask2_2 = _mm256_cmpeq_epi16(b2_shed, long_comment_end);

        u32 longcomm_m1_1 = (u32) _mm256_movemask_epi8(long_comment_end_mask1_1);
        u32 longcomm_m1_2 = (u32) _mm256_movemask_epi8(long_comment_end_mask1_2);
        u32 longcomm_m2_1 = (u32) _mm256_movemask_epi8(long_comment_end_mask2_1);
        u32 longcomm_m2_2 = (u32) _mm256_movemask_epi8(long_comment_end_mask2_2);
        u32 longcomm_mmask_1 = longcomm_m1_1 | (longcomm_m2_1 >> 1);
        u32 longcomm_mmask_2 = longcomm_m1_2 | (longcomm_m2_2 >> 1);
        u64 longcomm_mmask = ((u64) longcomm_mmask_1) | ((u64) longcomm_mmask_2) << 32;
#endif

        lexbuf newline_1 = _mm256_cmpeq_epi8(b_1, cmpmask_newline);
        lexbuf newline_2 = _mm256_cmpeq_epi8(b_2, cmpmask_newline);

        u32 toks_mmask_1 = (u32) _mm256_movemask_epi8(toks_mask_1);
        u32 toks_mmask_2 = (u32) _mm256_movemask_epi8(toks_mask_2);
        u32 idents_m1_1 = (u32) _mm256_movemask_epi8(nidents_mask1_1);
        u32 idents_m1_2 = (u32) _mm256_movemask_epi8(nidents_mask1_2);
        u32 idents_m2_1 = (u32) _mm256_movemask_epi8(nidents_mask2_1);
        u32 idents_m2_2 = (u32) _mm256_movemask_epi8(nidents_mask2_2);
        u32 newline_mmask_1 = (u32) _mm256_movemask_epi8(newline_1);
        u32 newline_mmask_2 = (u32) _mm256_movemask_epi8(newline_2);
        u32 idents_mmask_1 = idents_m1_1 | (idents_m2_1 >> 1);
        u32 idents_mmask_2 = idents_m1_2 | (idents_m2_2 >> 1);

        // Avoid branching - if last char of first buf is an alhpa, then second
        // part can't start with ident. Since we disable the first bit of the
        // second part, we and with bit-flipped 1 or 0.
        u32 fixup_mmask_1 = _mm256_movemask_epi8(idents_mask_1);
        u32 fixup_mmask_2 = _mm256_movemask_epi8(idents_mask_2);
        idents_mmask_2 &= ~((u32) ((fixup_mmask_1 & (((u32) 1) << 31)) != 0));

        u32 idents_fullmask_rev_1 = ~((u32) _mm256_movemask_epi8(idents_mask_1));
        u32 idents_fullmask_rev_2 = ~((u32) _mm256_movemask_epi8(idents_mask_2));
        u32 common_mmask_1 = toks_mmask_1 | idents_mmask_1 | newline_mmask_1;
        u32 common_mmask_2 = toks_mmask_2 | idents_mmask_2 | newline_mmask_2;

        ASSERT((toks_mmask_1 & idents_mmask_1) == 0);
        ASSERT((toks_mmask_2 & idents_mmask_2) == 0);

        u64 newline_mmask = ((u64) newline_mmask_1) | ((u64) newline_mmask_2) << 32;
        u64 idents_mmask = ((u64) idents_mmask_1) | ((u64) idents_mmask_2) << 32;
        u64 idents_fullmask_rev = ((u64) idents_fullmask_rev_1) | ((u64) idents_fullmask_rev_2) << 32;
        u64 fixup_mmask = ((u64) fixup_mmask_1) | ((u64) fixup_mmask_2) << 32;
        u64 common_mmask = ((u64) common_mmask_1) | ((u64) common_mmask_2) << 32;
        u64 s = common_mmask;

        if (carry == CARRY_IDENT && (idents_mmask & 1))
        {
            // Ignore the token, b/c this is just the continuation
            s = (s & (s - 1));

            u64 mask = idents_fullmask_rev & (~1);
            i32 wsidx = mask ? __builtin_ctzll(mask) : 64;
            NOOPTIMIZE(wsidx); /* TODO */

            printf("Token ignored L += %d\n", wsidx);
        }

        // Could be also set after the loop, it makes no difference and we will
        // probably get better caching here?

        // NOTE, TODO: Based on some real code: CARRY_IDENT happens ~58% and
        // CARRY_NONE ~26% which leaves CARRY_OP with ~16%. TODO: Optimize to
        // get advantage of this!
        if (fixup_mmask & ((u64)1 << 63)) carry = CARRY_IDENT;
        else carry = CARRY_NONE;

#if PRINT_LINES
        {
            printf("|");
            for (int i = 0; i < 2 * nlex; ++i)
                printf("%d", i % 10);
            printf("|\n");
            printf("|");
            for (int i = 0; i < 2 * nlex; ++i)
            {
                if (p[i] == '\n') printf(" ");
                else if (!p[i]) printf(" ");
                else printf("%c", p[i]);
            }
            printf("|\n");
            printf("|");
            for (int i = 0; i < 2 * nlex; ++i)
            {
                if (common_mmask & ((u64)1 << i)) printf("*");
                else printf(" ");
            }
            printf("|\n");
            printf("\n");
        }
#endif

        while (s)
        {
            i32 idx = __builtin_ctzll(s);
            char* x = p + idx;
            s = (s & (s - 1));

            // TODO: Make sure that the next is also a character, not any match?
            u64 size_ge_2 = !s || (s & ((u64)1 << (idx + 1))); /* TODO: Overflow shift? */

            if (newline_mmask & ((u64)1 << idx)) // TODO: Unlikely? Perhaps not so much?
            {
                curr_line++;
                curr_inline_idx = -idx;

                continue;
            }

            if (idents_mmask & ((u64)1 << idx)) // TODO: digit
            {
                ASSERT(('a' <= x[0] && x[0] <= 'z')
                       || ('A' <= x[0] && x[0] <= 'Z')
                       || x[0] == '_'
                       || ('0' <= x[0] && x[0] <= '9'));

                i32 wsidx = 64; /* TODO: no branch? */
                if (LIKELY(idx < 63))
                {
                    u64 mask = idents_fullmask_rev & (~(((u64) 1 << (idx + 1)) - 1)); // TODO: Overflow shift?
                    wsidx = mask ? __builtin_ctzll(mask) : 64;
                }
                NOOPTIMIZE(wsidx); /* TODO */

                printf("%s:%ld:%ld: TOK (L = %d): \"",
                       g_fname, curr_line, curr_inline_idx + idx, wsidx - idx);
                printf("%.*s", wsidx - idx, x);
                printf("\"\n");
                if ('0' <= x[0] && x[0] <= '9') n_parsed_numbers++;
                else n_parsed_idents++;
            }
            else if (x[0] == '"') // TODO: probably move down? Does it matter?
            {
                printf("%s:%ld:%ld: TOK: STRING START\n", g_fname, curr_line, curr_inline_idx + idx);
                curr_inline_idx += idx + 1; // TODO: try to avoid here?
                char* strstart = x++;

                // TODO: Don't increment index in the loop, calculate adv from the start?

                for (;; x += sizeof(lexbuf))
in_string:
                {
                    lexbuf nb = _mm256_loadu_si256((void *)x);
                    lexbuf nb_n = _mm256_cmpeq_epi8(nb, cmpmask_doublequote);
                    lexbuf nl_n = _mm256_cmpeq_epi8(nb, cmpmask_newline);
                    u32 nb_mm = _mm256_movemask_epi8(nb_n);
                    u32 nl_mm = _mm256_movemask_epi8(nl_n);

                    // TODO: Also, validate utf here? Maybe not?

                    // If there is a newline in the buffer, check if it in in
                    // the doublequote and make sure it is backslashed
                    if (UNLIKELY(nl_mm))
                    {
                        i32 end_idx = nb_mm ? __builtin_ctz(nb_mm) : 32;
                        while (nl_mm)
                        {
                            i32 nl_idx = __builtin_ctz(nl_mm);
                            if (nl_idx > end_idx)
                                break;

                            nl_mm = (nl_mm & (nl_mm - 1));
                            if (x[nl_idx - 1] == '\\')
                            {
                                // TODO: Copypaste Could be extracted to a function
                                char* b = &x[nl_idx - 2];
                                int bslashes = 1;
                                while (*b-- == '\\')
                                    bslashes++;

                                if (bslashes & 1)
                                {
                                    printf("There are %d backslashes, so doublequote is cancelled-out\n", bslashes);
                                    strstart = x + nl_idx;
                                    curr_line++;
                                    curr_inline_idx = 1;
                                    continue;
                                }
                                else
                                {
                                    printf("There are %d backslashes, this is a valid doublequote\n", bslashes);
                                }
                            }

                            printf("Unescaped newline in the middle of the string\n");
                            exit(1);
                        }
                    }

repeat_doublequote_seek:
                    if (nb_mm)
                    {
                        i32 end_idx = __builtin_ctz(nb_mm);

#if 0 // TODO: Revive this?
                        if (UNLIKELY(nl_end_idx < end_idx))
                        {
                            printf("Newline is before doublequote, lex error?\n");
                            exit(1);
                            // goto lex error
                        }
#endif

                        if (UNLIKELY(x[end_idx - 1] == '\\'))
                        {
                            // TODO: Could be extracted to a function
                            char* b = &x[end_idx - 2];
                            int bslashes = 1;
                            while (*b-- == '\\')
                                bslashes++;

                            if (bslashes & 1)
                            {
                                printf("There are %d backslashes, so doublequote is cancelled-out\n", bslashes);
                                nb_mm = (nb_mm & (nb_mm - 1));
                                goto repeat_doublequote_seek;
                            }
                            else
                            {
                                printf("There are %d backslashes, this is a valid doublequote\n", bslashes);
                            }
                        }

                        p = x + end_idx + 1;
                        curr_inline_idx += (x - strstart) - 1 + end_idx + 1;
                        carry = CARRY_NONE;
                        n_parsed_strings++;
                        goto continue_outer;
                    }
                }
            }
            else if (x[0] == '\'')
            {
                u64 skip_idx = 2 + (x[1] == '\\');
                u64 skip_mask = (1 << (skip_idx + 1)) - 2;
                if (UNLIKELY(skip_idx == 2 && x[2] != '\''))
                {
                    printf("BAD: \"%.*s\"\n", 8, x);
                    NOTREACHED;
                }

                printf("%s:%ld:%ld: TOK SINGLE CHAR: \"%.*s\"\n",
                       g_fname, curr_line, curr_inline_idx + idx,
                       x[1] != '\\' ? 1 : 2, x + 1);

                // TODO: NOTREACHED if bad sequence
                NOOPTIMIZE(skip_mask);
                n_parsed_chars++;
                if (idx + skip_idx >= 64)
                {
                    p += idx + skip_idx + 1;
                    curr_inline_idx += idx + skip_idx + 1;
                    goto continue_outer;
                }

                s &= (~(skip_mask << idx));
                continue;
            }
            else
            {
                /* This may give false positives */
                // TODO: 0b is gnu extension
                // TODO: Is this even correct? what about -> ?
                // TODO: Narrow that criteria
                if (size_ge_2 /* && (x[0] == x[1] || x[1] == '=' || (x[1] & 0b11111011) == ':') */)
                {
                    u32 w = u32_loadu(x);
                    u32 w2 = w & 0xFFFF;
                    u32 w3 = w & 0xFFFFFF;

                    if (w2 == TOK2('/', '/'))
                    {
                        printf("Skip // comment\n");
                        u64 m = s & newline_mmask;
                        if (m) // Short skip, newline in the same buf
                        {
                            printf("  short comment\n");
                            i32 comment_end_idx = __builtin_ctzll(m);
                            if (UNLIKELY(p[comment_end_idx - 1] == '\\')) // TODO: Use vectors instead of looking up?
                            {
                                // TODO: This is the place, where warning about
                                // multi-lne-single-line comment can be emmited.
                                printf("We actually have a backslashed string, "
                                       "so don't bother with a fast skip\n");
                                goto skip_long;
                            }

                            s &= ~(((u64)1 << comment_end_idx) - 1);
                            n_single_comments++;
                            n_single_comments_skipped++;
                            continue;
                        }

skip_long:
                        printf("  long comment\n");
                        p = x + 1;
                        for (;; p += sizeof(lexbuf))
in_short_comment:
                        {
                            if (UNLIKELY(p >= string_end))
                            {
                                printf("Cannot parse further, breaking and saving state!\n");
                                state->in = IN_SHORT_COMMENT;
                                goto finalize;
                            }

#if PRINT_LINES
                            {
                                printf("|");
                                for (int i = 0; i < nlex; ++i)
                                    printf("%d", i % 10);
                                printf("|\n");
                                printf("|");
                                for (int i = 0; i < nlex; ++i)
                                {
                                    if (p[i] == '\n') printf(" ");
                                    else if (!p[i]) printf(" ");
                                    else printf("%c", p[i]);
                                }
                                printf("|\n");
                                printf("\n");
                            }
#endif


                            lexbuf cb = _mm256_loadu_si256((void*) p);
                            lexbuf cb_n = _mm256_cmpeq_epi8(cb, cmpmask_newline);
                            u32 cb_mm = _mm256_movemask_epi8(cb_n);
                            if (cb_mm)
                            {
                                p += __builtin_ctzll(cb_mm);
                                break;
                            }
                        }

                        p++;
                        curr_line++;
                        curr_inline_idx = 1;
                        carry = CARRY_NONE;
                        n_single_comments++;
                        goto continue_outer;
                    }
                    else if (w2 == TOK2('/', '*'))
                    {
                        printf("%s:%ld:%ld: /* comment", g_fname, curr_line, curr_inline_idx + idx);

                        // TODO: Potencially passing 0 to ctz
                        u64 c = s & longcomm_mmask;
                        if (c && __builtin_ctzll(c) < __builtin_ctzll(s & newline_mmask))
                        {
                            printf("  (short)\n");
                            n_long_comments_skipped++;

                            i32 tz = __builtin_ctzll(c);
                            u64 adv_pow = (u64)1 << (tz + 1); // TODO: OVERFLOW HERE
                            s &= ~(adv_pow | (adv_pow - 1));

                            continue;
                        }
                        else
                        {
                            p = x + 2;
                            curr_inline_idx += idx + 2;
                            printf("  (long)\n");
                            for (;;)
                            {
                                lexbuf comment_end = _mm256_set1_epi16((u16) TOK2('*', '/'));
                                lexbuf cb_1 = _mm256_loadu_si256((void*) p);
#if 0
                                lexbuf cb_2 = mm_ext_shl8_si256(cb_1);
#else
                                lexbuf cb_2 = _mm256_loadu_si256((void*) (p + 1)); // TODO: This is incurrect
#endif

                                lexbuf cb_end_1 = _mm256_cmpeq_epi16(cb_1, comment_end);
                                lexbuf cb_end_2 = _mm256_cmpeq_epi16(cb_2, comment_end);
                                lexbuf cb_nl = _mm256_cmpeq_epi8(cb_1, cmpmask_newline);
                                u32 cb_mm_1 = _mm256_movemask_epi8(cb_end_1);
                                u32 cb_mm_2 = _mm256_movemask_epi8(cb_end_2);
                                u32 cb_nl_mm = _mm256_movemask_epi8(cb_nl);

                                // TODO: NEXT: Test both cases
                                u32 m1 = cb_mm_1 & 0b10101010101010101010101010101010;
                                u32 m2 = cb_mm_2 & 0b01010101010101010101010101010101;

#if PRINT_LINES
                                {
                                    printf("|");
                                    for (int i = 0; i < nlex; ++i)
                                        printf("%d", i % 10);
                                    printf("|\n");
                                    printf("|");
                                    for (int i = 0; i < nlex; ++i)
                                    {
                                        if (p[i] == '\n') printf(" ");
                                        else if (!p[i]) printf(" ");
                                        else printf("%c", p[i]);
                                    }
                                    printf("|\n");
                                    printf("\n");
                                }
#endif

                                u32 cb_mm = m1 | (m2 << 1);
                                if (cb_mm)
                                {
                                    i32 tz = __builtin_ctz(cb_mm);
                                    printf("tz = %d %d\n", tz, ((((u32)1 << tz) & (m2 << 1)) != 0));

                                    i32 adv = tz + 1 + ((((u32)1 << tz) & (m2 << 1)) != 0);
                                    u32 adv_pow = (u32)1 << tz;
                                    cb_nl_mm &= adv_pow | (adv_pow - 1);
                                    if (cb_nl_mm)
                                    {
                                        i32 nladv = 32 - __builtin_clz(cb_nl_mm);
                                        ASSERT(nladv < adv);
                                        curr_line += __builtin_popcount(cb_nl_mm);
                                        curr_inline_idx = 1 + (adv - nladv);
                                    }
                                    else
                                    {
                                        curr_inline_idx += adv;
                                    }

                                    p += adv;
                                    break;
                                }

                                if (cb_nl_mm)
                                {
                                    curr_line += __builtin_popcount(cb_nl_mm);
                                    curr_inline_idx = __builtin_clz(cb_nl_mm) + 1;
                                }
                                else
                                {
                                    curr_inline_idx += 32;
                                }

                                p += sizeof(lexbuf);
                            }

                            // curr_inline_idx += 2;
                            // p += 2;
                            carry = CARRY_NONE;
                            n_long_comments++;
                            goto continue_outer;
                        }
                    }
                    else if (    w3 == TOK3('>', '>', '=')
                                 || (w3 == TOK3('<', '<', '='))
                                 || (w3 == TOK3('.', '.', '.')))
                    {
                        printf("%s:%ld:%ld: TOK3: \"%.*s\"\n",
                               g_fname, curr_line, curr_inline_idx + idx, 3, x);

                        // TODO: move outer for common code?
                        u64 skip_idx = 2;
                        u64 skip_mask = (1 << (skip_idx + 1)) - 2;
                        if (idx + skip_idx >= 64)
                        {
                            p += idx + skip_idx + 1;
                            curr_inline_idx += idx + skip_idx + 1;
                            goto continue_outer;
                        }

                        s &= (~(skip_mask << idx));
                        continue;
                    }
                    else if ((   w2 == TOK2('+', '+'))
                             || (w2 == TOK2('-', '-'))
                             || (w2 == TOK2('-', '>'))
                             || (w2 == TOK2('<', '<'))
                             || (w2 == TOK2('>', '>'))
                             || (w2 == TOK2('&', '&'))
                             || (w2 == TOK2('|', '|'))
                             || (w2 == TOK2('<', '='))
                             || (w2 == TOK2('>', '='))
                             || (w2 == TOK2('=', '='))
                             || (w2 == TOK2('!', '='))
                             || (w2 == TOK2('+', '='))
                             || (w2 == TOK2('-', '='))
                             || (w2 == TOK2('*', '='))
                             || (w2 == TOK2('/', '='))
                             || (w2 == TOK2('%', '='))
                             || (w2 == TOK2('&', '='))
                             || (w2 == TOK2('^', '='))
                             || (w2 == TOK2('|', '='))
                             || (w2 == TOK2('?', ':')))
                    {
                        // TODO: This should match */, because if we find this token
                        // here it means we should end parsing with a lexing error
                        printf("%s:%ld:%ld: TOK2: \"%.*s\"\n",
                               g_fname, curr_line, curr_inline_idx + idx, 2, x);

                        // TODO: move outer for common code?
                        u64 skip_idx = 1;
                        u64 skip_mask = (1 << (skip_idx + 1)) - 2;
                        if (idx + skip_idx >= 64)
                        {
                            p += idx + skip_idx + 1;
                            curr_inline_idx += idx + skip_idx + 1;
                            goto continue_outer;
                        }

                        s &= (~(skip_mask << idx));
                        continue;
                    }
                }

                NOOPTIMIZE(x[0]);
                printf("%s:%ld:%ld: TOK: \"%c\"\n", g_fname, curr_line, curr_inline_idx + idx, x[0]); /* TODO: idx + 1 > 32 */
            }
        }

        p += 2 * nlex;
        curr_inline_idx += 2 * nlex; /* TODO: lexbuf size */
continue_outer:
    }

finalize:
    // Update state:
    // state->string;
    // state->string_end;
    // TODO: set additional state in comment/in string etc
    state->curr_line = curr_line;
    state->curr_inline_idx = curr_inline_idx;
    state->carry = carry;

    return p;
}

int
main(int argc, char** argv)
{
    if (argc < 2) fatal("Need a file");
    char* fname = argv[1];
    g_fname = fname; // TODO: Temporary

#if 0
    FILE* f = fopen(fname, "rb");
    fseek(f, 0, SEEK_END);
    isize fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* string = calloc(fsize /* + nlex ROUND UP TO SSE BUF SIZE, TODO */, sizeof(char));
    fread(string, 1, fsize, f);
#else
    int fd = open(fname, O_RDONLY);
    struct stat st;
    fstat(fd, &st); /* Get the size of the file. */ // TODO: Check retval of stat and fail
    isize fsize = st.st_size;
    char* string = (char *) mmap(0, fsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
#endif

    ASSERT(fsize > 64); // TODO: !!!!
    lex_state state;
    state.string = string;
    state.string_end = string + fsize - 64; // TODO: DOn't hardcode 64
    state.curr_line = 1;
    state.curr_inline_idx = 1;
    state.carry = CARRY_NONE;
    state.in = IN_NONE;

    char* p = lex(&state);
    int offset = p - state.string_end;
    char string_tail[64 + 64]; // TODO: Don't hardcode!
    memset(string_tail, ' ', sizeof(string_tail));
    memcpy(string_tail, state.string_end + offset, 64 - offset);

    state.string = string_tail;
    state.string_end = string_tail + 64 - offset;

#if PRINT_LINES
    printf("----------------------------------------------------------------\n");
    {
        printf("|");
        for (u32 i = 0; i < sizeof(string_tail) - offset; ++i)
            printf("%d", i % 10);
        printf("|\n|");
        for (u32 i = 0; i < sizeof(string_tail) - offset; ++i)
        {
            if (string_tail[i] == '\n') printf(" ");
            else if (!string_tail[i]) printf(" ");
            else printf("%c", string_tail[i]);
        }
        printf("|\n");
    }
#endif
    p = lex(&state);

    fprintf(stdout, "Parsed: "
            "%ld lines, %ld ids, %ld strings, "
            "%ld chars, %ld ints, %ld hex,   %ld floats,    "
            "%ld //s, %ld /**/s,   0 #foo\n",
            state.curr_line, n_parsed_idents, n_parsed_strings,
            n_parsed_chars, n_parsed_numbers, 0L, 0L,
            n_single_comments, n_long_comments);

#if 0
    fclose(f);
#else
#endif
    return 0;
}
