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

/* #undef ASSERT */
/* #define ASSERT(...) */
/* #define printf(...) ((void) 0) */
/* #define NOOPTIMIZE(EXPR) asm volatile (""::"g"(&EXPR):"memory"); */
/* #define PRINT_LINES 0 */
#define NOOPTIMIZE(EXPR) ((void) 0)
#define PRINT_LINES 1

#define TOK2(X, Y) (((u32) X) | ((u32) Y) << 8)
#define TOK3(X, Y, Z) (((u32) X) | ((u32) Y) << 8 | ((u32) Z) << 16)
#define FNAME "./test.c" // TODO: temp

typedef __m256i lexbuf;
static const int nlex = sizeof(lexbuf);

enum carry
{
    CARRY_NONE = 0,
    CARRY_IDENT = 1,
    CARRY_OP = 2,
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

int
main(int argc, char** argv)
{
    if (argc < 2) fatal("Need a file");
    char* fname = argv[1];

#if 0
    FILE* f = fopen(fname, "rb");
    fseek(f, 0, SEEK_END);
    isize fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* string = calloc(fsize + nlex/* ROUND UP TO SSE BUF SIZE, TODO */, sizeof(char));
    char* p = string;
    fread(string, 1, fsize, f);
#else
    isize fsize;
    char* string;
    struct stat st;
    int fd = open(fname, O_RDONLY);

    /* Get the size of the file. */
    fstat(fd, &st); // TODO: Check retval of stat and fail
    fsize = st.st_size;

    string = (char *) mmap(0, fsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    for (i64 i = fsize - nlex; i < fsize; ++i)
        string[i] = 0;
    fsize -= nlex;
    char* p = string;
#endif

    i64 curr_line = 1;
    i64 curr_inline_idx = 1;
    i32 carry = CARRY_NONE;
    i64 n_parsed_idents = 0;
    i64 n_parsed_numbers = 0;
    i64 n_parsed_strings = 0;
    i64 n_parsed_chars = 0;
    i64 n_single_comments = 0;
    i64 n_long_comments = 0;

    // TODO: Probably move into the loop
    lexbuf cmpmask_0 = _mm256_set1_epi8('0' - 1);
    lexbuf cmpmask_9 = _mm256_set1_epi8('9' + 1);
    lexbuf cmpmask_a = _mm256_set1_epi8('a' - 1);
    lexbuf cmpmask_z = _mm256_set1_epi8('z' + 1);
    lexbuf cmpmask_A = _mm256_set1_epi8('A' - 1);
    lexbuf cmpmask_Z = _mm256_set1_epi8('Z' + 1);
    lexbuf cmpmask_underscore = _mm256_set1_epi8('_');
    lexbuf cmpmask_newline = _mm256_set1_epi8('\n');
    lexbuf cmpmask_char_start = _mm256_set1_epi8(0x20);

    for (;;)
    continue_outer:
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

        // NOTE, TODO: Based on some real code: CARRY_IDENT happens ~58% and
        // CARRY_NONE ~26% which leaves CARRY_OP with ~16%. TODO: Optimize to
        // get advantage of this!
        if (carry == CARRY_IDENT && (idents_mmask & 1))
        {
            // Ignore the token, b/c this is just the continuation
            s = (s & (s - 1));

            u64 mask = idents_fullmask_rev & (~1);
            i32 wsidx = mask ? __builtin_ctzll(mask) : 64;
            NOOPTIMIZE(wsidx); /* TODO */

            printf("Token ignored L += %d\n", wsidx);
        }
#if 0
        else if (carry == CARRY_OP && (toks_mmask_1 & 1))
        {
            // TODO: Implement all the cases!
            // TODO: this only works because / is never at the end of any token!
            printf("Perhaps missing a token\n");

            if (p[-1] == '/' && p[0] == '/')
            {
                p++;
                goto skip_single_line_comment;
            }

            if (p[-1] == '/' && p[0] == '*')
            {
                p++;
                goto skip_multi_line_comment;
            }
        }
#endif

        // Could be also set after the loop, it makes no difference and we will
        // probably get better caching here?

        // NOTE, TODO: Based on some real code: CARRY_IDENT happens ~58% and
        // CARRY_NONE ~26% which leaves CARRY_OP with ~16%. TODO: Optimize to
        // get advantage of this!
        if (fixup_mmask & ((u64)1 << 63)) carry = CARRY_IDENT;
#if 0
        else if ((common_mmask & ((u64)1 << 63)) && !(newline_mmask & ((u64)1 << 63))) carry = CARRY_OP;
#endif
        else carry = CARRY_NONE;

        while (s)
        {
            i32 idx = __builtin_ctzll(s);
            char* x = p + idx;
            s = (s & (s - 1));
            u64 size_ge_2 = !s || (s & ((u64)1 << (idx + 1))); /* TODO: Overflow shift */

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
                    u64 mask = idents_fullmask_rev & (~(((u64) 1 << (idx + 1)) - 1));
                    wsidx = mask ? __builtin_ctzll(mask) : 64;
                }
                NOOPTIMIZE(wsidx); /* TODO */

                printf("%s:%ld:%ld: TOK (L = %d): \"",
                       FNAME, curr_line, curr_inline_idx + idx, wsidx - idx);
                printf("%.*s", wsidx - idx, x);
                printf("\"\n");
                if ('0' <= x[0] && x[0] <= '9') n_parsed_numbers++;
                else n_parsed_idents++;
            }
            else if (x[0] == '"')
            {
                // TODO: probably move down?
                printf("%s:%ld:%ld: TOK: STRING START\n",
                       FNAME, curr_line, curr_inline_idx + idx);
                // TODO: This doesn't work with newlines
                char* save = x++;
                while (*x != '"') /* TODO: Incorrect! */
                    x++;
                curr_inline_idx += idx + (x - save) + 1;
                p = x + 1;
                carry = CARRY_NONE;
                n_parsed_strings++;
                goto continue_outer;
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
#if 0
            else if (size_ge_2 && x[0] == '/' && x[1] == '/')
            {
                // No need to calculate curr_inline_idx, it ends with a \n

                // TODO: why it even works?
                printf("Skip // comment\n");
                p = x + 1;
                goto skip_single_line_comment;
            }
            else if (size_ge_2 && x[0] == '/' && x[1] == '*')
            {
                printf("Skip /* comment\n");
                p = x + 1; // TODO: Why +1, not +2 ??
                curr_inline_idx += idx + 1;
                goto skip_multi_line_comment;
            }
#endif
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
                    // u16 w = *((u16*) x); /* TODO: Portable unaligned load */
#if 0
                    if (x[2] == '=' && (w2 == TOK2('<', '<') || w2 == TOK2('>', '>')))
                    {
                        printf("%s:%ld:%ld: TOK3: \"%.*s\"\n",
                               FNAME, curr_line, curr_inline_idx + idx, 3, x);
                        s ^= ((u64)1 << (idx + 1)) | ((u64)1 << (idx + 2));
                        continue;
                    }
                    else
#endif
                    if (w2 == TOK2('/', '/'))
                    {
                        printf("Skip // comment\n");
                        p = x + 1;
                        goto skip_single_line_comment;
                    }
                    else if (w2 == TOK2('/', '*'))
                    {
                        printf("Skip /* comment\n");
                        p = x + 1; // TODO: Why +1, not +2 ??
                        curr_inline_idx += idx + 1;
                        goto skip_multi_line_comment;
                    }
                    else if (    w3 == TOK3('>', '>', '=')
                             || (w3 == TOK3('<', '<', '='))
                             || (w3 == TOK3('.', '.', '.')))
                    {
                        printf("%s:%ld:%ld: TOK3: \"%.*s\"\n",
                               FNAME, curr_line, curr_inline_idx + idx, 3, x);

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
                               FNAME, curr_line, curr_inline_idx + idx, 2, x);

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
                printf("%s:%ld:%ld: TOK: \"%c\"\n", FNAME, curr_line, curr_inline_idx + idx, x[0]); /* TODO: idx + 1 > 32 */
            }
        }

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

        p += 2 * nlex;
        curr_inline_idx += 2 * nlex; /* TODO: lexbuf size */

        if (p - string >= fsize) break;
        continue;

skip_single_line_comment:
        {
            while (*p != '\n') /* TODO: Incorrect! */
                p++;

            p++;
            curr_line++;
            curr_inline_idx = 1;
            carry = CARRY_NONE;
            n_single_comments++;
            continue;
        }

skip_multi_line_comment:
        {
            while (*p != '*' || *(p + 1) != '/') /* TODO: Incorrect! */
            {
                if (*p == '\n')
                {
                    curr_line++;
                    curr_inline_idx = 0;
                }
                p++;
                curr_inline_idx++;
            }

            curr_inline_idx += 2;
            p += 2;
            carry = CARRY_NONE;
            n_long_comments++;
            continue;
        }
    }

    fprintf(stdout, "Parsed: "
            "%ld lines, %ld ids, %ld strings, "
            "%ld chars, %ld ints, %ld hex,   %ld floats,    "
            "%ld //s, %ld /**/s,   0 #foo\n",
            curr_line, n_parsed_idents, n_parsed_strings,
            n_parsed_chars, n_parsed_numbers, 0L, 0L,
            n_single_comments, n_long_comments);

#if 0
    fclose(f);
#else
#endif
    return 0;
}
