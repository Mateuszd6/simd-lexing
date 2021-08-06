// TODO: \-NL is broken on windows, because the newlines there are \-CR-NL
// TODO: 64 and 32 are hardcoded everywhere
// TODO: CHeck for stray characters; < 9(HT) or > 13(CR) or 127 (DEL)
// TODO: Treat characters with first bit set as valid parts of identfier (utf8)

extern char const* __asan_default_options(void); /* TODO: remove */
extern char const* __asan_default_options() { return "detect_leaks=0"; }

/* TODO: These go to the library */
#include <string.h> /* memset, memcpy */
#include <stddef.h> /* size_t, ptrdiff_t */
#include <stdint.h> /* (u)int(8,16,32,64) */

typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;
typedef size_t usize;
typedef ptrdiff_t isize;

#ifdef _MSC_VER
#  include <intrin.h>
#endif
#include <immintrin.h>

/* TODO: use user-defined assert? */
#include <assert.h>
#define ASSERT assert

/* TODO: */
/* #define LEX_ON_TOKEN(TOKEN_TYPE, VALUE) */

/* TODO: Remove this, because in the library code, there should be no printfs */
#include <stdio.h> /* printf, fprintf */

#if (defined(__GNUC__) || defined(__clang__))
#  define LIKELY(EXPR) __builtin_expect((EXPR), 1)
#  define UNLIKELY(EXPR) __builtin_expect((EXPR), 0)
#else
#  define LIKELY(EXPR) (EXPR)
#  define UNLIKELY(EXPR) (EXPR)
#endif

// TODO: MSVC version of notreached __assume(0)
#if (defined(__GNUC__) || defined(__clang__))
#  define NOTREACHED __builtin_unreachable()
#else
#  define NOTREACHED ((void) 0)
#endif

/* TODO: Remove these! */
#ifdef RELEASE
#  undef ASSERT
#  define ASSERT(...)
#  define printf(...) ((void) 0)
#  if (defined(__GNUC__)) || (defined(__clang__))
#    define NOOPTIMIZE(EXPR) asm volatile (""::"g"(&EXPR):"memory")
#  else
#    define NOOPTIMIZE(EXPR) (void) (EXPR)
#  endif
#  define PRINT_LINES 0
#else
#  define NOOPTIMIZE(EXPR) ((void) 0)
#  define PRINT_LINES 1
#endif

#if (defined(__GNUC__)) || (defined(__clang__))
#  define ctz32(V) (__builtin_ctz((uint32_t)(V)))
#  define ctz64(V) (__builtin_ctzll((uint64_t)(V)))
#  define clz32(V) (__builtin_clz((uint32_t)(V)))
#  define clz64(V) (__builtin_clzll((uint64_t)(V)))
#  define popcnt(V) (__builtin_popcount(V))
#  define popcnt64(V) (__builtin_popcountll(V))
#elif (defined(_MSC_VER))
#  pragma intrinsic(_BitScanForward)
#  pragma intrinsic(_BitScanForward64)
#  pragma intrinsic(_BitScanReverse)
#  pragma intrinsic(_BitScanReverse64)
#  define ctz32(V) (intr_msvc_ctz32((uint32_t) V))
#  define ctz64(V) (intr_msvc_ctz64((uint64_t) V))
#  define clz32(V) (31 - intr_msvc_clz32((uint32_t) V))
#  define clz64(V) (63 - intr_msvc_clz64((uint64_t) V))
#  define popcnt64(V) (__popcnt64((uint64_t) V))
#  define popcnt(V) (__popcnt((uint32_t) V))

__forceinline static int32_t
intr_msvc_ctz32(uint32_t mask)
{
    unsigned long index = 0;
    _BitScanForward(&index, mask);
    return (int32_t) index;
}
__forceinline static int32_t
intr_msvc_ctz64(uint64_t mask)
{
    unsigned long index = 0;
    _BitScanForward64(&index, mask);
    return (int32_t) index;
}
__forceinline static int32_t
intr_msvc_clz32(uint32_t mask)
{
    unsigned long index = 0;
    _BitScanReverse(&index, mask);
    return (int32_t) index;
}
__forceinline static int32_t
intr_msvc_clz64(uint64_t mask)
{
    unsigned long index = 0;
    _BitScanReverse64(&index, mask);
    return (int32_t) index;
}
#endif

#define TOK2(X, Y) (((u32) X) | ((u32) Y) << 8)
#define TOK3(X, Y, Z) (((u32) X) | ((u32) Y) << 8 | ((u32) Z) << 16)

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

enum lex_error
{
    OK, /* OK */
    ERR_NOMEM, /* Out of memory */
    ERR_BAD_CHARACTER, /* Char that should not appear in a file */
    ERR_BAD_CHAR_LITERAL, /* Char literal other than 'x', '\x' or '\xxx' */
    ERR_UNEXPECTED_COMMENT_END, /* Encoutered * / while not being in comment */
    ERR_NEWLINE_IN_STRING, /* NL char without backslash in string */
    ERR_EOF_AT_COMMENT, /* EOF without ending a comment */
    ERR_EOF_AT_STRING, /* EOF without ending a string */
    ERR_EOF_AT_CHAR, /* EOF without ending a '' char literal */
};

char const* lex_error_str[] = {
    [OK] = "OK",
    [ERR_NOMEM] = "Out of memory",
    [ERR_BAD_CHARACTER] = "Unexpected character in file",
    [ERR_BAD_CHAR_LITERAL] = "Invalid character literal",
    [ERR_UNEXPECTED_COMMENT_END] = "Unexpected comment end",
    [ERR_NEWLINE_IN_STRING] = "Unescaped newline in string",
    [ERR_EOF_AT_COMMENT] = "Unexpected EOF in comment",
    [ERR_EOF_AT_STRING] = "Unexpected EOF in string",
    [ERR_EOF_AT_CHAR] = "Unexpected EOF in character literal", /* TODO: Test that, is it even possible? */
};

static inline __m256i
mm_ext_shl8_si256(__m256i a)
{
    __m256i mask = _mm256_permute2x128_si256(a, a, _MM_SHUFFLE(0, 0, 3, 0));
    return _mm256_alignr_epi8(a, mask, 16-1);
}

/* TODO: Macro define this? */
static inline u32
u32_loadu(void const* p)
{
    u32 retval;
    memcpy(&retval, p, sizeof retval);

    return retval;
}

// TODO: Get rid of them!
#if 1
static i64 n_parsed_idents = 0;
static i64 n_parsed_numbers = 0;
static i64 n_parsed_strings = 0;
static i64 n_parsed_chars = 0;
static i64 n_single_comments = 0;
static i64 n_long_comments = 0;
static char* g_fname = 0;
#endif

typedef struct lex_state lex_state;
struct lex_state
{
    char const* string;
    char const* string_end;
    i32 curr_line;
    i32 curr_inline_idx;
    i32 carry;
    i32 out_in;
    char const* out_at;
};

typedef struct lex_result lex_result;
struct lex_result
{
    i32 err;
    i32 curr_line;
    i32 curr_inline_idx;
};

typedef struct token token;
struct token
{
    union
    {
        char str[16];
        u64 d[2];
        char* ptr;
    } u;

    i32 line;
    i32 idx;
    i32 len;
};

static i32
lex_s(lex_state* state)
{
    char const* p = state->string;
    char const* string_end = state->string_end;
    i32 curr_line = state->curr_line;
    i32 curr_inline_idx = state->curr_inline_idx;
    i32 carry = state->carry;
    i32 in = IN_NONE;
    i32 error = OK;

    __m256i cmpmask_0 = _mm256_set1_epi8('0' - 1);
    __m256i cmpmask_9 = _mm256_set1_epi8('9' + 1);
    __m256i cmpmask_a = _mm256_set1_epi8('a' - 1);
    __m256i cmpmask_z = _mm256_set1_epi8('z' + 1);
    __m256i cmpmask_A = _mm256_set1_epi8('A' - 1);
    __m256i cmpmask_Z = _mm256_set1_epi8('Z' + 1);
    __m256i cmpmask_underscore = _mm256_set1_epi8('_');
    /* TODO: Add ability to also use these in token */
    /* __m256i cmpmask_sinlequote = _mm256_set1_epi8('\''); */
    /* __m256i cmpmask_dollar = _mm256_set1_epi8('$'); */
    __m256i cmpmask_newline = _mm256_set1_epi8('\n');
    __m256i cmpmask_doublequote = _mm256_set1_epi8('\"');
    __m256i cmpmask_char_start = _mm256_set1_epi8(0x20); /* space - 1 */

    while (p < string_end)
    {
        __m256i b_1 = _mm256_loadu_si256((void*) p);
        __m256i b_2 = _mm256_loadu_si256((void*) (p + sizeof(__m256i)));

        __m256i charmask_1 = _mm256_cmpgt_epi8(b_1, cmpmask_char_start);
        __m256i charmask_2 = _mm256_cmpgt_epi8(b_2, cmpmask_char_start);

        __m256i mask1_1 = _mm256_and_si256(_mm256_cmpgt_epi8(b_1, cmpmask_0), _mm256_cmpgt_epi8(cmpmask_9, b_1));
        __m256i mask2_1 = _mm256_and_si256(_mm256_cmpgt_epi8(b_1, cmpmask_a), _mm256_cmpgt_epi8(cmpmask_z, b_1));
        __m256i mask3_1 = _mm256_and_si256(_mm256_cmpgt_epi8(b_1, cmpmask_A), _mm256_cmpgt_epi8(cmpmask_Z, b_1));
        __m256i mask4_1 = _mm256_cmpeq_epi8(b_1, cmpmask_underscore);

        __m256i mask1_2 = _mm256_and_si256(_mm256_cmpgt_epi8(b_2, cmpmask_0), _mm256_cmpgt_epi8(cmpmask_9, b_2));
        __m256i mask2_2 = _mm256_and_si256(_mm256_cmpgt_epi8(b_2, cmpmask_a), _mm256_cmpgt_epi8(cmpmask_z, b_2));
        __m256i mask3_2 = _mm256_and_si256(_mm256_cmpgt_epi8(b_2, cmpmask_A), _mm256_cmpgt_epi8(cmpmask_Z, b_2));
        __m256i mask4_2 = _mm256_cmpeq_epi8(b_2, cmpmask_underscore);

        __m256i idents_mask_1 = _mm256_or_si256(_mm256_or_si256(mask1_1, mask2_1), _mm256_or_si256(mask3_1, mask4_1));
        __m256i idents_mask_2 = _mm256_or_si256(_mm256_or_si256(mask1_2, mask2_2), _mm256_or_si256(mask3_2, mask4_2));

        // TODO: Try not to shift it
        __m256i idents_mask_shed_1 = mm_ext_shl8_si256(idents_mask_1);
        __m256i idents_mask_shed_2 = mm_ext_shl8_si256(idents_mask_2);

        __m256i toks_mask_1 = _mm256_andnot_si256(idents_mask_1, charmask_1);
        __m256i toks_mask_2 = _mm256_andnot_si256(idents_mask_2, charmask_2);

        __m256i ident_start = _mm256_set1_epi16((u16) 0xFF00);
        __m256i idents_startmask1_1 = _mm256_cmpeq_epi16(idents_mask_1, ident_start);
        __m256i idents_startmask2_1 = _mm256_cmpeq_epi16(idents_mask_shed_1, ident_start);
        __m256i idents_startmask1_2 = _mm256_cmpeq_epi16(idents_mask_2, ident_start);
        __m256i idents_startmask2_2 = _mm256_cmpeq_epi16(idents_mask_shed_2, ident_start);

        __m256i nidents_mask1_1 = _mm256_and_si256(idents_mask_1, idents_startmask1_1);
        __m256i nidents_mask2_1 = _mm256_and_si256(idents_mask_shed_1, idents_startmask2_1);
        __m256i nidents_mask1_2 = _mm256_and_si256(idents_mask_2, idents_startmask1_2);
        __m256i nidents_mask2_2 = _mm256_and_si256(idents_mask_shed_2, idents_startmask2_2);
        __m256i newline_1 = _mm256_cmpeq_epi8(b_1, cmpmask_newline);
        __m256i newline_2 = _mm256_cmpeq_epi8(b_2, cmpmask_newline);

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

        /* If previous frame finished with an ident and this one starts with
         * one, it's a continuation of an old ident */
        if (carry == CARRY_IDENT && (idents_mmask & 1))
        {
            s = (s & (s - 1)); /* Ignore first token */

            u64 mask = idents_fullmask_rev & (~1);
            i32 wsidx = mask ? ctz64(mask) : 64;
            NOOPTIMIZE(wsidx); /* TODO */

            printf("Token ignored L += %d\n", wsidx);
        }

        /* Based on some real code, CARRY_IDENT happens in ~78% cases */
        /* TODO: This can be branchless */
        if (fixup_mmask & ((u64)1 << 63)) carry = CARRY_IDENT;
        else carry = CARRY_NONE;

#if PRINT_LINES
        {
            printf("|");
            for (int i = 0; i < 64; ++i)
                printf("%d", i % 10);
            printf("|\n");
            printf("|");
            for (int i = 0; i < 64; ++i)
            {
                if (p[i] == '\n') printf(" ");
                else if (!p[i]) printf(" ");
                else printf("%c", p[i]);
            }
            printf("|\n");
            printf("|");
            for (int i = 0; i < 64; ++i)
            {
                if (common_mmask & ((u64)1 << i)) printf("*");
                else printf(" ");
            }
            printf("|\n");
        }
#endif

        while (s)
        {
            i32 idx = ctz64(s);
            char const* x = p + idx;
            s = (s & (s - 1));

            /* Shift by idx + 1 is actually well defined, because if idx is 63,
               it means that s is 0 and we won't check the second condition */
            u64 size_ge_2 = !s || (s & ((u64)1 << (idx + 1)));

            if (newline_mmask & ((u64)1 << idx))
            {
                curr_line++;
                curr_inline_idx = -idx;

                continue;
            }

            if (idents_mmask & ((u64)1 << idx)) // TODO: digit
            {
                ASSERT(x[0] == '_'
                       || ('a' <= x[0] && x[0] <= 'z')
                       || ('A' <= x[0] && x[0] <= 'Z')
                       || ('0' <= x[0] && x[0] <= '9'));

                i32 wsidx = 64; /* TODO: no branch? */
                if (LIKELY(idx < 63))
                {
                    u64 mask = idents_fullmask_rev & (~(((u64) 1 << (idx + 1)) - 1)); // TODO: Overflow shift?
                    wsidx = mask ? ctz64(mask) : 64;
                }
                NOOPTIMIZE(wsidx); /* TODO */

                printf("%s:%d:%d: TOK (L = %d): \"",
                       g_fname, curr_line, curr_inline_idx + idx, wsidx - idx);
                printf("%.*s", wsidx - idx, x);
                printf("\"\n");
                if ('0' <= x[0] && x[0] <= '9') n_parsed_numbers++;
                else n_parsed_idents++;
            }
            else if (x[0] == '"') // TODO: probably move down? Does it matter?
            {
                printf("%s:%d:%d: TOK: STRING START\n", g_fname, curr_line, curr_inline_idx + idx);
                curr_inline_idx += idx + 1; // TODO: try to avoid here?
                char const* strstart = x++;

                // TODO: Don't increment index in the loop, calculate adv from the start?
                for (;; x += sizeof(__m256i))
                {
                    if (UNLIKELY(x >= string_end))
                    {
                        printf("Cannot parse further, breaking and saving state!\n");
                        in = IN_STRING;
                        p = x;
                        curr_inline_idx += x - strstart - 1;
                        carry = CARRY_NONE;
                        goto finalize;
                    }

                    __m256i nb = _mm256_loadu_si256((__m256i*) x);
                    __m256i nb_n = _mm256_cmpeq_epi8(nb, cmpmask_doublequote);
                    __m256i nl_n = _mm256_cmpeq_epi8(nb, cmpmask_newline);
                    u32 nb_mm = _mm256_movemask_epi8(nb_n);
                    u32 nl_mm = _mm256_movemask_epi8(nl_n);

                    // TODO: Also, validate utf here? Maybe not?

                    // If there is a newline in the buffer, check if it in in
                    // the doublequote and make sure it is backslashed
                    if (UNLIKELY(nl_mm))
                    {
                        i32 end_idx = nb_mm ? ctz32(nb_mm) : 32;
                        while (nl_mm)
                        {
                            i32 nl_idx = ctz32(nl_mm);
                            if (nl_idx > end_idx)
                                break;

                            nl_mm = (nl_mm & (nl_mm - 1));
                            if (LIKELY(x[nl_idx - 1] == '\\')) /* TODO: CR-NL! */
                            {
                                // TODO: Copypaste Could be extracted to a function
                                char const* b = &x[nl_idx - 2];
                                int bslashes = 1;
                                while (*b-- == '\\')
                                    bslashes++;

                                /* Odd number of backslashes, doublequote is cancelled out */
                                if (bslashes & 1)
                                {
                                    strstart = x + nl_idx;
                                    curr_line++;
                                    curr_inline_idx = 1;
                                    continue;
                                }
                            }

                            goto err_newline_in_string;
                        }
                    }

repeat_doublequote_seek:
                    if (nb_mm)
                    {
                        i32 end_idx = ctz32(nb_mm);
                        if (UNLIKELY(x[end_idx - 1] == '\\'))
                        {
                            // TODO: Could be extracted to a function
                            char const* b = &x[end_idx - 2];
                            int bslashes = 1;
                            while (*b-- == '\\')
                                bslashes++;

                            /* There is odd number of backslashes, so
                               doublequote is cancelled-out */
                            if (bslashes & 1)
                            {
                                nb_mm = (nb_mm & (nb_mm - 1));
                                goto repeat_doublequote_seek;
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
                u64 skip_idx = 2;
                if (x[1] == '\\')
                {
                    if (LIKELY(x[3] == '\''))
                    {
                        skip_idx = 3;
                    }
                    else
                    {
                        /* Support three-num character literals ('\001') */
                        /* TODO: Make sure this value is less than 128!! */
                        skip_idx = 5;
                        if (UNLIKELY(x[2] < '0')
                            || UNLIKELY(x[3] < '0')
                            || UNLIKELY(x[4] < '0')
                            || UNLIKELY(x[2] > '9')
                            || UNLIKELY(x[3] > '9')
                            || UNLIKELY(x[4] > '9')
                            || UNLIKELY(x[5] != '\''))
                        {
                            curr_inline_idx += idx;
                            goto err_bad_char_literal;
                        }
                    }
                }
                else if (UNLIKELY(x[2] != '\''))
                {
                    curr_inline_idx += idx;
                    goto err_bad_char_literal;
                }

                printf("%s:%d:%d: TOK SINGLE CHAR: \"%.*s\"\n",
                       g_fname, curr_line, curr_inline_idx + idx,
                       x[1] != '\\' ? 1 : 2, x + 1);

                u64 skip_mask = (1 << (skip_idx + 1)) - 2;
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
                // TODO: 0b is gnu extension
                if (size_ge_2)
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
                            i32 comment_end_idx = ctz64(m);
                            if (UNLIKELY(p[comment_end_idx - 1] == '\\')) /* TODO: CR-NL! */
                                goto skip_long;

                            s &= ~(((u64)1 << comment_end_idx) - 1);
                            n_single_comments++;
                            continue;
                        }

skip_long:
                        printf("  long comment\n");
                        p = x + 1;
                        for (;; p += sizeof(__m256i))
                        {
                            if (UNLIKELY(p >= string_end))
                            {
                                printf("Cannot parse further, breaking and saving state!\n");
                                in = IN_SHORT_COMMENT;
                                carry = CARRY_NONE;
                                goto finalize;
                            }

#if PRINT_LINES
                            {
                                printf("|");
                                for (int i = 0; i < 32; ++i)
                                    printf("%d", i % 10);
                                printf("|\n");
                                printf("|");
                                for (int i = 0; i < 32; ++i)
                                {
                                    if (p[i] == '\n') printf(" ");
                                    else if (!p[i]) printf(" ");
                                    else printf("%c", p[i]);
                                }
                                printf("|\n");
                                printf("\n");
                            }
#endif

                            __m256i cb = _mm256_loadu_si256((__m256i*) p);
                            __m256i cb_n = _mm256_cmpeq_epi8(cb, cmpmask_newline);
                            u32 cb_mm = _mm256_movemask_epi8(cb_n);

                            /* TODO: NEXT: Fix // \NL comments! */
                            if (cb_mm)
                            {
                                p += ctz64(cb_mm);
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
                        printf("%s:%d:%d: /* comment", g_fname, curr_line, curr_inline_idx + idx);
                        p = x + 2;
                        curr_inline_idx += idx + 2;
                        printf("  (long)\n");
                        for (;; p += sizeof(__m256i))
                        {
                            if (UNLIKELY(p >= string_end))
                            {
                                printf("Cannot parse further, breaking and saving state!\n");
                                in = IN_LONG_COMMENT;
                                carry = CARRY_NONE;
                                goto finalize;
                            }

                            __m256i comment_end = _mm256_set1_epi16((u16) TOK2('*', '/'));
                            // TODO: This is very misleading, cb_2 is _before_ cb_1 !!
                            __m256i cb_1 = _mm256_loadu_si256((void*) p);
                            __m256i cb_2 = _mm256_loadu_si256((void*) (p + 1));
                            __m256i cb_end_1 = _mm256_cmpeq_epi16(cb_1, comment_end);
                            __m256i cb_end_2 = _mm256_cmpeq_epi16(cb_2, comment_end);
                            __m256i cb_nl = _mm256_cmpeq_epi8(cb_1, cmpmask_newline);
                            u32 cb_mm_1 = _mm256_movemask_epi8(cb_end_1);
                            u32 cb_mm_2 = _mm256_movemask_epi8(cb_end_2);
                            u32 cb_nl_mm = _mm256_movemask_epi8(cb_nl);
                            u32 m1 = cb_mm_1 & 0b10101010101010101010101010101010; /* TODO: Don't use gnu's 0b */
                            u32 m2 = cb_mm_2 & 0b01010101010101010101010101010101;
#if PRINT_LINES
                            {
                                printf("|");
                                for (int i = 0; i < 32; ++i)
                                    printf("%d", i % 10);
                                printf("|\n");
                                printf("|");
                                for (int i = 0; i < 32; ++i)
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
                                i32 tz = ctz32(cb_mm);
                                printf("tz = %d %d\n", tz, ((((u32)1 << tz) & (m2 << 1)) != 0));
                                i32 adv = tz + 1 + ((((u32)1 << tz) & (m2 << 1)) != 0);
                                u32 adv_pow = (u32)1 << tz;
                                cb_nl_mm &= adv_pow | (adv_pow - 1);
                                if (cb_nl_mm)
                                {
                                    i32 nladv = 32 - clz32(cb_nl_mm);
                                    ASSERT(nladv < adv);
                                    curr_line += popcnt(cb_nl_mm);
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
                                curr_line += popcnt(cb_nl_mm);
                                curr_inline_idx = clz32(cb_nl_mm) + 1;
                            }
                            else
                            {
                                curr_inline_idx += 32;
                            }
                        }

                        // curr_inline_idx += 2;
                        // p += 2;
                        carry = CARRY_NONE;
                        n_long_comments++;
                        goto continue_outer;
                    }
                    else if (    w3 == TOK3('>', '>', '=')
                             || (w3 == TOK3('<', '<', '='))
                             || (w3 == TOK3('.', '.', '.')))
                    {
                        printf("%s:%d:%d: TOK3: \"%.*s\"\n",
                               g_fname, curr_line, curr_inline_idx + idx, 3, x);

                        u64 skip_idx = 2;
                        u64 skip_mask = ((u64)1 << (skip_idx + 1)) - 2;
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
                        /* TODO: This should match * /, because if we find this
                           token here it means we should end parsing with a
                           lexing error */
                        printf("%s:%d:%d: TOK2: \"%.*s\"\n",
                               g_fname, curr_line, curr_inline_idx + idx, 2, x);

                        u64 skip_idx = 1;
                        u64 skip_mask = ((u64)1 << (skip_idx + 1)) - 2;
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
                printf("%s:%d:%d: TOK: \"%c\"\n", g_fname, curr_line, curr_inline_idx + idx, x[0]);
            }
        }

        p += 64;
        curr_inline_idx += 64;
continue_outer:
        (void) 0;
    }

finalize:
    // Update state:
    state->curr_line = curr_line;
    state->curr_inline_idx = curr_inline_idx;
    state->carry = carry;
    state->out_in = in;
    state->out_at = p;

    return error;


err_bad_char_literal:
    error = ERR_BAD_CHAR_LITERAL;
    goto finalize;

err_newline_in_string:
    error = ERR_NEWLINE_IN_STRING;
    goto finalize;
}

lex_result
real_lex(char const* string, isize len)
{
    i32 err = OK;
    lex_state state;
    state.string = string;
    state.string_end = string + len - 64; /* TODO: Don't hardcode 64 */
    state.curr_line = 1;
    state.curr_inline_idx = 1;
    state.carry = CARRY_NONE;
    if (LIKELY(len > 64))
    {
        err = lex_s(&state);
        if (UNLIKELY(err != OK))
            goto finalize;
    }
    else
    {
        // Fixup state, because len turned out to be too small
        state.string_end = string + len;
        state.out_at = string;
        state.out_in = IN_NONE;
    }

    char const* p = state.out_at;
    i32 offset = (i32) (p - state.string_end);
    char string_tail[64 + 64]; /* TODO: Don't hardcode! */
    memset(string_tail, ' ', sizeof(string_tail));
    memcpy(string_tail, state.string_end + offset, 64 - offset);

    state.string = string_tail;
    state.string_end = string_tail + 64 - offset;
    if (UNLIKELY(state.out_in != IN_NONE))
    {
        switch (state.out_in) {
        case IN_STRING:
        {
            while (state.string < state.string_end && *state.string != '"') /* TODO: Check backqotes correctly! */
            {
                /* TODO: Check for buggy -1 when newline is at the beginning of the buffer: */
                if (UNLIKELY(*state.string == '\n' && *(state.string - 1) != '\\'))
                {
                    state.curr_inline_idx = 0;
                    state.curr_line++;
                }

                state.curr_inline_idx++;
                state.string++;
            }

            if (UNLIKELY(state.string >= state.string_end))
            {
                err = ERR_EOF_AT_STRING;
                goto finalize;
            }

            state.curr_inline_idx++;
            state.string++;
        } break;
        case IN_SHORT_COMMENT:
        {
            /* TODO: Check for buggy -1 when newline is at the beginning of the buffer */
            while (state.string < state.string_end && (*state.string != '\n' || *(state.string - 1) == '\\'))
                state.string++;
            if (UNLIKELY(state.string >= state.string_end))
            {
                err = ERR_EOF_AT_COMMENT;
                goto finalize;
            }

            state.curr_line++;
            state.curr_inline_idx = 1;
            state.string++;
        } break;
        case IN_LONG_COMMENT:
        {
            while (state.string < state.string_end - 1 && (*state.string != '*' || *(state.string + 1) != '/'))
            {
                if (UNLIKELY(*state.string == '\n'))
                {
                    state.curr_inline_idx = 0;
                    state.curr_line++;
                }

                state.curr_inline_idx++;
                state.string++;
            }

            if (UNLIKELY(state.string >= state.string_end - 1))
            {
                err = ERR_EOF_AT_COMMENT;
                goto finalize;
            }

            state.curr_inline_idx += 2;
            state.string += 2;
        } break;
        default: NOTREACHED;
        }
    }

#if 0 && PRINT_LINES
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

    err = lex_s(&state);
    p = state.out_at;
    if (UNLIKELY(state.out_in != IN_NONE))
    {
        switch (state.out_in) {
        case IN_STRING:
        {
            err = ERR_EOF_AT_STRING;
            goto finalize;
        } break;
        case IN_SHORT_COMMENT:
        {
            err = ERR_EOF_AT_COMMENT;
            goto finalize;
        } break;
        case IN_LONG_COMMENT:
        {
            err = ERR_EOF_AT_COMMENT;
            goto finalize;
        } break;
        default: NOTREACHED;
        }
    }

    fprintf(stdout, "Parsed: "
            "%d lines, %ld ids, %ld strings, "
            "%ld chars, %ld ints, %ld hex,   %ld floats,    "
            "%ld //s, %ld /**/s,   0 #foo\n",
            state.curr_line, n_parsed_idents, n_parsed_strings,
            n_parsed_chars, n_parsed_numbers, 0L, 0L,
            n_single_comments, n_long_comments);
finalize:
    lex_result retval;
    retval.err = err;
    retval.curr_line = state.curr_line;
    retval.curr_inline_idx = state.curr_inline_idx;

    return retval;
}

/* */
/* TODO: These go to example file! */
/* */
#define USE_MMAP 1

#ifdef _MSC_VER
#  define _CRT_SECURE_NO_DEPRECATE
#endif
#include <stdio.h>

#if USE_MMAP
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#endif

int
main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "%s: fatal: Need a file", argv[0]);
        exit(1);
    }

    char* fname = argv[1];
    g_fname = fname; // TODO: Temporary

#if !(USE_MMAP)
    FILE* f = fopen(fname, "rb");
    if (UNLIKELY(!f))
    {
        fprintf(stderr, "%s: error: Could not open file '%s' for reading\n", argv[0], fname);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    isize fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* string = (char*) malloc(fsize);
    fread(string, 1, fsize, f);
#else
    int fd = open(fname, O_RDONLY);
    if (UNLIKELY(fd == -1))
    {
        fprintf(stderr, "%s: error: Could not open file '%s' for reading\n", argv[0], fname);
        exit(1);
    }

    struct stat st;
    fstat(fd, &st); /* Get the size of the file. */ // TODO: Check retval of stat and fail
    isize fsize = st.st_size;
    char* string = (char *) mmap(0, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
#endif

    lex_result r = real_lex(string, fsize);
    if (r.err != OK)
    {
        fprintf(stderr, "%s:%d:%d: error: %s\n", fname, r.curr_line, r.curr_inline_idx, lex_error_str[r.err]);
        exit(1);
    }

    return 0;
}
