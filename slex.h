/* TODO: However, probably verify if the unicode is correct (like in simdjson?)
 * TODO: Lexing float better in SIMD code (can be way faster!)
 * TODO: Use simd when advancing over floats
 * TODO: Add ability to add singlequote and backtick strings and maybe include
 *       dollars and single quotes in the ident?
 * TODO: Prefix the API!
 */

#ifndef SLEX_H_
#define SLEX_H_

/* Define ON_TOKEN_CB _before_ including this file, to a function that is called
 * on each token. For performance reasons, the function should be a static
 * inline and be defined in a same translation unit (that's the point, an API
 * with a function pointer is slower, even if function is static and defined in
 * the same file!). Callback should have a signature:

 * void func(char const* s, i32 x, i32 type, i32 line, i32 idx, void* user)
 * where:
 *   s and x vary based on a type:
 *     * TODO: Describe them
 *   type is a token type enum
 *   line is a line number
 *   idx is a column number withing this line (starting from 1)
 *   user is an arbitrary user data that gets passed in
 *
 * DEFINE:
 *   U32_LOADU - to a func with a signature uint32_t u32_loadu(void const* p),
 *   which is used by the lexer to read unaligned dword from memory pointed by p.
 *   If none is provided, a reasonable (memcpy) default is definde and used.
 */

#include <string.h> /* memset, memcpy */
#include <stddef.h> /* size_t, ptrdiff_t */
#include <stdint.h> /* (u)int(8,16,32,64) */

#ifdef _MSC_VER
#  include <intrin.h>
#endif

#ifdef __AVX2__ /* TODO: Check for Visual Studio */
#  include <immintrin.h>
#endif

/* Intrinsics: */
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
#  define ctz32(V) (msvc_ctz32((uint32_t) V))
#  define ctz64(V) (msvc_ctz64((uint64_t) V))
#  define clz32(V) (31 - msvc_msb32((uint32_t) V))
#  define clz64(V) (63 - msvc_msb64((uint64_t) V))
#  define popcnt64(V) (__popcnt64((uint64_t) V))
#  define popcnt(V) (__popcnt((uint32_t) V))
__forceinline static int32_t
msvc_ctz32(uint32_t mask)
{
    unsigned long index = 0;
    _BitScanForward(&index, mask);
    return (int32_t) index;
}
__forceinline static int32_t
msvc_ctz64(uint64_t mask)
{
    unsigned long index = 0;
    _BitScanForward64(&index, mask);
    return (int32_t) index;
}
__forceinline static int32_t
msvc_msb32(uint32_t mask)
{
    unsigned long index = 0;
    _BitScanReverse(&index, mask);
    return (int32_t) index;
}
__forceinline static int32_t
msvc_msb64(uint64_t mask)
{
    unsigned long index = 0;
    _BitScanReverse64(&index, mask);
    return (int32_t) index;
}
#endif

#ifndef U32_LOADU
#  define U32_LOADU u32_loadu
static inline uint32_t
u32_loadu(void const* p)
{
    uint32_t retval;
    memcpy(&retval, p, sizeof retval);

    return retval;
}
#endif

/* TODO: Remove these! */
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

#if (defined(__GNUC__) || defined(__clang__))
#  define LIKELY(EXPR) __builtin_expect((EXPR), 1)
#  define UNLIKELY(EXPR) __builtin_expect((EXPR), 0)
#else
#  define LIKELY(EXPR) (EXPR)
#  define UNLIKELY(EXPR) (EXPR)
#endif

#if (defined(__GNUC__) || defined(__clang__))
#  define NOTREACHED __builtin_unreachable()
#elif (defined(_MSC_VER))
#  define NOTREACHED __assume(0)
#else
#  define NOTREACHED ((void) 0)
#endif

/* TODO: Remove these! */
#ifdef RELEASE
#  undef ASSERT
#  define ASSERT(...)
#  if (defined(__GNUC__)) || (defined(__clang__))
#    define NOOPTIMIZE(EXPR) __asm__ volatile (""::"g"(&EXPR):"memory")
#  else
#    define NOOPTIMIZE(EXPR) (void) (EXPR)
#  endif
#else
#  define NOOPTIMIZE(EXPR) ((void) 0)
#endif

/* TODO: Remove!! */
#if PRINT_LINES
#  include <stdio.h>
#endif

#define TOK1(X) ((u32) X)
#define TOK2(X, Y) (((u32) X) | ((u32) Y) << 8)
#define TOK3(X, Y, Z) (((u32) X) | ((u32) Y) << 8 | ((u32) Z) << 16)
#define TOK_ANY(P, LEN) (((u32) (P)[0]) | ((u32) (LEN > 1 ? (P)[1] : 0)) << 8 | ((u32) (LEN > 2 ? (P)[2] : 0)) << 16)

#define DEF_TOK2(X, Y) ||(w2 == TOK2(X, Y))
#define DEF_TOK3(X, Y, Z) ||(w3 == TOK3(X, Y, Z))

#define TOK_BYTEMASK(X) ((X) < (1 << 8) ? 0xFF : ((X) < (1 << 16) ? 0xFFFF : 0xFFFFFF))
#define TOK_NBYTES(X) ((X) < (1 << 8) ? 1 : ((X) < (1 << 16) ? 2 : 3))

#define CALL_USER(STR, LEN, TYPE, LINE, IDX, USER)                             \
    do {                                                                       \
        int user_error = ON_TOKEN_CB(STR, LEN, TYPE, LINE, IDX, USER);         \
        if (UNLIKELY(user_error) != 0)                                         \
            goto err_user;                                                     \
    } while (0)

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
    IN_IDENT = 4,
};

enum lex_error
{
    OK = 0, /* OK */
    ERR_NOMEM = 1, /* Out of memory */
    ERR_BAD_CHARACTER = 2, /* Char that should not appear in a file */
    ERR_BAD_CHAR_LITERAL = 3, /* Char literal other than 'x', '\x' or '\xxx' */
    ERR_UNEXPECTED_COMMENT_END = 4, /* Encoutered * / while not being in comment */
    ERR_NEWLINE_IN_STRING = 5, /* LF char without backslash in string */
    ERR_EOF_AT_COMMENT = 6, /* EOF without ending a comment */
    ERR_EOF_AT_STRING = 7, /* EOF without ending a string */
    ERR_USER = 8, /* Error in user callback */
};

char const* lex_error_str[] = {
    "OK",
    "Out of memory",
    "Unexpected character in file",
    "Invalid character literal",
    "Unexpected comment end",
    "Unescaped newline in string",
    "Unexpected EOF in comment",
    "Unexpected EOF in string",
};

enum token_type
{
    T_IDENT,
    T_OP,
    T_INTEGER,
    T_FLOAT,
    T_CHAR,
    T_DOUBLEQ_STR,
};

typedef struct lex_impl_result lex_impl_result;
struct lex_impl_result
{
    char const* out_at;
    i32 err;
    i32 curr_line;
    i32 curr_idx;
    i32 carry; /* TODO: try to reduce? */
    i32 carry_tok_len;
};

typedef struct lex_result lex_result;
struct lex_result
{
    i32 err;
    i32 curr_line;
    i32 curr_idx;
};

/* TODO: Use it in "simple" mode */
typedef union token_val token_val;
union token_val
{
    char* ident;
    char* integer;
    char* str;
    u32 op;
    u32 ch;
    double floating;
};

#ifdef __AVX2__
static inline __m256i
mm_ext_shl8_si256(__m256i a)
{
    __m256i mask = _mm256_permute2x128_si256(a, a, _MM_SHUFFLE(0, 0, 3, 0));
    return _mm256_alignr_epi8(a, mask, 16-1);
}
#endif

static inline i32
count_backslashes(char const* p)
{
    i32 backslashes = 0;
    p -= (*p == '\r' ? 1 : 0); /* Handle CR-LF */
    while (*p-- == '\\')
        backslashes++;

    return backslashes;
}

#ifdef __AVX2__
static lex_impl_result
lex_s(char const* string, char const* string_end, void* user)
{
    char const* p = string;
    i32 curr_line = 1;
    i32 curr_idx = 1;
    i32 carry = CARRY_NONE;
    i32 carry_tok_len = 0;
    i32 error = OK;

    __m256i cmpmask_0 = _mm256_set1_epi8('0' - 1);
    __m256i cmpmask_9 = _mm256_set1_epi8('9' + 1);
    __m256i cmpmask_a = _mm256_set1_epi8('a' - 1);
    __m256i cmpmask_z = _mm256_set1_epi8('z' + 1);
    __m256i cmpmask_A = _mm256_set1_epi8('A' - 1);
    __m256i cmpmask_Z = _mm256_set1_epi8('Z' + 1);
    __m256i cmpmask_underscore = _mm256_set1_epi8('_');
    __m256i cmpmask_newline = _mm256_set1_epi8('\n');
    __m256i cmpmask_doublequote = _mm256_set1_epi8('"');
    __m256i cmpmask_char_start = _mm256_set1_epi8(0x20); /* space - 1 */

    __m256i stray_char_1 = _mm256_set1_epi8(9); /* HT */
    __m256i stray_char_2 = _mm256_set1_epi8(13); /* CR */
    __m256i stray_char_3 = _mm256_set1_epi8(' ');
    __m256i stray_char_4 = _mm256_set1_epi8(127); /* DEL */

continue_outer:
    while (p < string_end)
    {
        __m256i b_1 = _mm256_loadu_si256((__m256i const*) p);
        __m256i b_2 = _mm256_loadu_si256((__m256i const*) (p + sizeof(__m256i)));

        __m256i charmask_1 = _mm256_cmpgt_epi8(b_1, cmpmask_char_start);
        __m256i charmask_2 = _mm256_cmpgt_epi8(b_2, cmpmask_char_start);

        __m256i mask1_1 = _mm256_and_si256(
            _mm256_cmpgt_epi8(b_1, cmpmask_0), _mm256_cmpgt_epi8(cmpmask_9, b_1));
        __m256i mask2_1 = _mm256_and_si256(
            _mm256_cmpgt_epi8(b_1, cmpmask_a), _mm256_cmpgt_epi8(cmpmask_z, b_1));
        __m256i mask3_1 = _mm256_and_si256(
            _mm256_cmpgt_epi8(b_1, cmpmask_A), _mm256_cmpgt_epi8(cmpmask_Z, b_1));
        __m256i mask4_1 = _mm256_cmpeq_epi8(b_1, cmpmask_underscore);

        __m256i mask1_2 = _mm256_and_si256(
            _mm256_cmpgt_epi8(b_2, cmpmask_0), _mm256_cmpgt_epi8(cmpmask_9, b_2));
        __m256i mask2_2 = _mm256_and_si256(
            _mm256_cmpgt_epi8(b_2, cmpmask_a), _mm256_cmpgt_epi8(cmpmask_z, b_2));
        __m256i mask3_2 = _mm256_and_si256(
            _mm256_cmpgt_epi8(b_2, cmpmask_A), _mm256_cmpgt_epi8(cmpmask_Z, b_2));
        __m256i mask4_2 = _mm256_cmpeq_epi8(b_2, cmpmask_underscore);

        /* Check out for stray characters: < 9(HT) or > 13(CR) or 127 (DEL) and
         * unicode (because cmpqt is signed, so utf8 characters are treated as
         * negative values and considered less than 9. */
        __m256i stray_mask_1_1 = _mm256_cmpgt_epi8(stray_char_1, b_1);
        __m256i stray_mask_1_2 = _mm256_cmpgt_epi8(stray_char_1, b_2);
        __m256i stray_mask_2_1 = _mm256_cmpgt_epi8(b_1, stray_char_2);
        __m256i stray_mask_2_2 = _mm256_cmpgt_epi8(b_2, stray_char_2);
        __m256i stray_mask_3_1 = _mm256_cmpgt_epi8(stray_char_3, b_1);
        __m256i stray_mask_3_2 = _mm256_cmpgt_epi8(stray_char_3, b_2);
        __m256i stray_mask_4_1 = _mm256_cmpeq_epi8(b_1, stray_char_4);
        __m256i stray_mask_4_2 = _mm256_cmpeq_epi8(b_2, stray_char_4);

        __m256i idents_mask_1 = _mm256_or_si256(
            _mm256_or_si256(mask1_1, mask2_1), _mm256_or_si256(mask3_1, mask4_1));
        __m256i idents_mask_2 = _mm256_or_si256(
            _mm256_or_si256(mask1_2, mask2_2), _mm256_or_si256(mask3_2, mask4_2));

        u32 stray_mmask_1 = _mm256_movemask_epi8(
            _mm256_or_si256(
                _mm256_or_si256(stray_mask_1_1, stray_mask_4_1),
                _mm256_and_si256(stray_mask_2_1, stray_mask_3_1)));
        u32 stray_mmask_2 = _mm256_movemask_epi8(
            _mm256_or_si256(
                _mm256_or_si256(stray_mask_1_2, stray_mask_4_2),
                _mm256_and_si256(stray_mask_2_2, stray_mask_3_2)));

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

        /* Avoid branching - if last char of first buf is an alhpa, then second part can't
         * start with ident. Since we disable the first bit of the second part, we and
         * with bit-flipped 1 or 0. */
        u32 fixup_mmask_1 = _mm256_movemask_epi8(idents_mask_1);
        u32 fixup_mmask_2 = _mm256_movemask_epi8(idents_mask_2);
        idents_mmask_2 &= ~((u32) ((fixup_mmask_1 & (((u32) 1) << 31)) != 0 ? 1 : 0));

        u32 idents_fullmask_rev_1 = ~((u32) _mm256_movemask_epi8(idents_mask_1));
        u32 idents_fullmask_rev_2 = ~((u32) _mm256_movemask_epi8(idents_mask_2));
        u32 common_mmask_1 = toks_mmask_1 | idents_mmask_1 | newline_mmask_1 | stray_mmask_1;
        u32 common_mmask_2 = toks_mmask_2 | idents_mmask_2 | newline_mmask_2 | stray_mmask_2;

        ASSERT((toks_mmask_1 & idents_mmask_1) == 0);
        ASSERT((toks_mmask_2 & idents_mmask_2) == 0);

        u64 newline_mmask = ((u64) newline_mmask_1) | ((u64) newline_mmask_2) << 32;
        u64 idents_mmask = ((u64) idents_mmask_1) | ((u64) idents_mmask_2) << 32;
        u64 idents_fullmask_rev = ((u64) idents_fullmask_rev_1) | ((u64) idents_fullmask_rev_2) << 32;
        u64 stray_mmask = ((u64) stray_mmask_1) | ((u64) stray_mmask_2) << 32;
        u64 fixup_mmask = ((u64) fixup_mmask_1) | ((u64) fixup_mmask_2) << 32;
        u64 common_mmask = ((u64) common_mmask_1) | ((u64) common_mmask_2) << 32;
        u64 s = common_mmask;

        /* If previous frame finished with an ident and this one starts with one, it's a
         * continuation of an old ident */
        if (carry == CARRY_IDENT)
        {
            /* It's a revert of ident mask, so we commpare to 0! */
            if (LIKELY(idents_fullmask_rev != ((u32) 0)))
            {
                i32 addidx = 0;
                if (idents_mmask & 1)
                {
                    s = (s & (s - 1)); /* Ignore first token */
                    u64 mask = idents_fullmask_rev & (~1);
                    addidx = mask ? ctz64(mask) : 64;
                }

                char tok_start = *(p - carry_tok_len);
                i32 tok_type = ('0' <= tok_start && tok_start <= '9') ? T_INTEGER : T_IDENT;
                if (tok_type == T_INTEGER && UNLIKELY(*(p + addidx) == '.'))
                {
                    /* TODO: Copypaste, do the same in "slow mode" */
                    addidx++;
                    for (; p - carry_tok_len + addidx < string_end
                             && (('0' <= *(p + addidx) && *(p + addidx) <= '9')
                                 || ('a' <= *(p + addidx) && *(p + addidx) <= 'z')
                                 || ('A' <= *(p + addidx) && *(p + addidx) <= 'Z')
                                 || *(p + addidx) == '.');
                         addidx++)
                    {
                        /* NOP */
                    }

                    if (UNLIKELY(p - carry_tok_len + addidx >= string_end))
                    {
                        p -= carry_tok_len;

                        /* Will break out of the loop and will be reported in the "slow" */
                        goto continue_outer;
                    }

                    CALL_USER(p - carry_tok_len, carry_tok_len + addidx, T_FLOAT,
                                curr_line, curr_idx - carry_tok_len, user);
                    carry = CARRY_NONE;
                    curr_idx += addidx;
                    p += addidx;
                    goto continue_outer;
                }

                CALL_USER(p - carry_tok_len, carry_tok_len + addidx, tok_type,
                            curr_line, curr_idx - carry_tok_len, user);
            }
            else
            {
                /* This is a very unlikely case; we've loaded a buffer, which is whole a
                 * continuation of current token, and is not finished */
                s = (s & (s - 1)); /* Ignore first token */
                carry_tok_len += 64;
            }
        }

        /* This way it can be branchless (evaluating boolean expression): */
        ASSERT(CARRY_NONE == 0);
        ASSERT(CARRY_IDENT == 1);
        carry = ((fixup_mmask & ((u64)1 << 63)) != 0);
        /* Based on some real code, CARRY_IDENT happens in ~78% cases */

#if PRINT_LINES
        {
            printf("|");
            for (int i = 0; i < 64; ++i)
                printf("%d", i % 10);
            printf("|\n");
            printf("|");
            for (int i = 0; i < 64; ++i)
            {
                if (p[i] == '\n' || p[i] == '\t') printf(" ");
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
            printf("|");
            for (int i = 0; i < 64; ++i)
            {
                if (stray_mmask & ((u64)1 << i)) printf("*");
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

            if (UNLIKELY(stray_mmask & ((u64)1 << idx)))
            {
                curr_idx += idx;
                goto err_bad_character;
            }

            if (newline_mmask & ((u64)1 << idx))
            {
                curr_line++;
                curr_idx = -idx;

                continue;
            }

            if (idents_mmask & ((u64)1 << idx))
            {
                ASSERT(x[0] == '_'
                       || ('a' <= x[0] && x[0] <= 'z')
                       || ('A' <= x[0] && x[0] <= 'Z')
                       || ('0' <= x[0] && x[0] <= '9'));

                i32 wsidx = 64;
                if (LIKELY(idx < 63))
                {
                    /* No overflow, because idx < 64 */
                    u64 mask = idents_fullmask_rev & (~(((u64) 1 << (idx + 1)) - 1));
                    wsidx = mask ? ctz64(mask) : 64;
                }

                /* If ident spans more than one buffer, will be reported later */
                if (s || carry == CARRY_NONE)
                {
                    i32 tok_type = ('0' <= x[0] && x[0] <= '9') ? T_INTEGER : T_IDENT;
                    if (tok_type == T_INTEGER && UNLIKELY(x[wsidx - idx] == '.'))
                    {
                        i32 old_wsidx = wsidx++;
                        for (; x + wsidx - idx < string_end
                                 && (('0' <= x[wsidx - idx] && x[wsidx - idx] <= '9')
                                     || ('a' <= x[wsidx - idx] && x[wsidx - idx] <= 'z')
                                     || ('A' <= x[wsidx - idx] && x[wsidx - idx] <= 'Z')
                                     || x[wsidx - idx] == '.');
                             wsidx++)
                        {
                            /* NOP */
                        }

                        if (UNLIKELY(x + wsidx - idx >= string_end))
                        {
                            /* TODO: Make sure these are calculated properly! */
                            carry = CARRY_NONE;
                            curr_idx += old_wsidx;
                            p = x;

                            /* Will break out of the loop and will be reported in the "slow" */
                            goto continue_outer;
                        }

                        CALL_USER(x, wsidx - idx, T_FLOAT, curr_line, curr_idx + idx, user);
                        carry = CARRY_NONE;
                        curr_idx += wsidx;
                        p = x + wsidx - idx;
                        goto continue_outer;
                    }

                    CALL_USER(x, wsidx - idx, tok_type, curr_line, curr_idx + idx, user);
                }
                else
                {
                    /* Delay reporting a token */
                    carry_tok_len = wsidx - idx;
                }
            }
            else if (x[0] == '"')
            {
                curr_idx += idx + 1;
                i32 str_start_line = curr_line;
                i32 str_start_idx = curr_idx;
                char const* strstart = x++;

                for (;; x += sizeof(__m256i))
                {
                    if (UNLIKELY(x >= string_end))
                    {
                        carry = CARRY_NONE;
#if 0
                        p = x;
                        /* These are only set in this specific case */
                        state->in_string_line = str_start_line;
                        state->in_string_idx = str_start_idx;
                        state->in_string_parsed = (i32)(p - strstart);
#else
                        p = strstart;
                        curr_line = str_start_line;
                        curr_idx = str_start_idx - 1;
#endif

                        goto finalize;
                    }

                    __m256i nb = _mm256_loadu_si256((__m256i*) x);
                    __m256i nb_n = _mm256_cmpeq_epi8(nb, cmpmask_doublequote);
                    __m256i nl_n = _mm256_cmpeq_epi8(nb, cmpmask_newline);
                    u32 nb_mm = _mm256_movemask_epi8(nb_n);
                    u32 nl_mm = _mm256_movemask_epi8(nl_n);
                    int nl_offset = 0;
                    u32 mask = nl_mm | nb_mm;
                    while (mask)
                    {
                        i32 mask_idx = mask ? ctz32(mask) : 32;
                        if (LIKELY(nb_mm & ((u32)1 << mask_idx)))
                        {
                            /* Doublequote, check if backslashed */
                            if (UNLIKELY(x[mask_idx - 1] == '\\')
                                || UNLIKELY(x[mask_idx - 2] == '\\'))
                            {
                                int bslashes = count_backslashes(&x[mask_idx - 1]);
                                if (bslashes & 1)
                                {
                                    mask = mask & (mask - 1);
                                    continue;
                                }
                            }

                            p = x + mask_idx + 1;
                            CALL_USER(strstart + 1, p - strstart - 2, T_DOUBLEQ_STR,
                                        str_start_line, str_start_idx, user);
                            curr_idx += mask_idx + 1 - nl_offset;
                            carry = CARRY_NONE;
                            goto continue_outer;
                        }
                        else
                        {
                            /* Newline, check if backslashed correctly */
                            if (LIKELY(x[mask_idx - 1] == '\\')
                                || LIKELY(x[mask_idx - 2] == '\\'))
                            {
                                int bslashes = count_backslashes(&x[mask_idx - 1]);
                                if (bslashes & 1) /* Odd number of backslashes cancels out */
                                {
                                    curr_line++;
                                    curr_idx = 1;
                                    nl_offset = mask_idx + 1;
                                    mask = mask & (mask - 1);
                                    continue;
                                }
                            }

                            curr_idx += mask_idx - nl_offset;
                            goto err_newline_in_string;
                        }
                    }

                    curr_idx += 32 - nl_offset;
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
                        skip_idx = 5;
                        if (UNLIKELY(x[2] < '0')
                            || UNLIKELY(x[3] < '0')
                            || UNLIKELY(x[4] < '0')
                            || UNLIKELY(x[2] > '9')
                            || UNLIKELY(x[3] > '9')
                            || UNLIKELY(x[4] > '9')
                            || UNLIKELY(x[5] != '\'')
                            /* Make sure val is < 256: */
                            || UNLIKELY(x[2] >= '2' && x[3] >= '5' && x[4] > '6'))
                        {
                            curr_idx += idx;
                            goto err_bad_char_literal;
                        }
                    }
                }
                else if (UNLIKELY(x[2] != '\''))
                {
                    curr_idx += idx;
                    goto err_bad_char_literal;
                }

                CALL_USER(x + 1, skip_idx - 1, T_CHAR, curr_line, curr_idx + idx, user);
                u64 skip_mask = (((u64) 1) << (skip_idx + 1)) - 2;
                if (idx + skip_idx >= 64)
                {
                    carry = CARRY_NONE;
                    p += idx + skip_idx + 1;
                    curr_idx += idx + skip_idx + 1;
                    goto continue_outer;
                }

                s &= (~(skip_mask << idx));
                continue;
            }
            else
            {
                /* Case when idx = 63 is actually well defined, because it means
                 * that s is 0 and we won't check the second condition */
                u64 size_ge_2 = !s || (s & ((u64)1 << (idx + 1)));

                u32 single_comment_bmask = TOK_BYTEMASK(SINGLELINE_COMMENT_START);
                u32 multi_comment_bmask = TOK_BYTEMASK(MULTILINE_COMMENT_START);

                u32 w = U32_LOADU(x);
                u32 w1 = w & 0xFF;
                u32 w2 = w & 0xFFFF;
                u32 w3 = w & 0xFFFFFF;

                if ((w & single_comment_bmask) == SINGLELINE_COMMENT_START)
                {
                    u64 m = s & newline_mmask;
                    if (m) /* Short skip, newline in the same buf */
                    {
                        i32 comment_end_idx = ctz64(m);
                        if (UNLIKELY(p[comment_end_idx - 1] == '\\')
                            || (UNLIKELY(p[comment_end_idx - 2] == '\\')))
                        {
                            /* Don't do fast skip, when we have some creepy \s next to
                             * the newline. Might give some false positives though. */
                            goto skip_long;
                        }

                        s &= ~(((u64)1 << comment_end_idx) - 1);
                        n_single_comments++;
                        continue;
                    }

skip_long:
                    p = x + 1;
                    i32 comm_start_line = curr_line;
                    i32 comm_start_idx = curr_idx + idx;
                    char const* commstart = p - 1;

                    for (;; p += sizeof(__m256i))
                    {
                        if (UNLIKELY(p >= string_end))
                        {
                            carry = CARRY_NONE;
                            p = commstart;
                            curr_line = comm_start_line;
                            curr_idx = comm_start_idx;

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

                        __m256i cbuf = _mm256_loadu_si256((__m256i*) p);
                        __m256i cbuf_n = _mm256_cmpeq_epi8(cbuf, cmpmask_newline);
                        u32 cbuf_mm = _mm256_movemask_epi8(cbuf_n);

repeat_nl_seek:
                        /* At least with gcc, backslash in multi-line single line
                         * comments cannot be escaped */
                        if (cbuf_mm)
                        {
                            i32 end_idx = ctz32(cbuf_mm);
                            if (UNLIKELY(p[end_idx - 1] == '\\')
                                || UNLIKELY(p[end_idx - 2] == '\\' && p[end_idx - 1] == '\r'))
                            {
                                curr_line++;
                                curr_idx = 1;
                                cbuf_mm = (cbuf_mm & (cbuf_mm - 1));
                                goto repeat_nl_seek;
                            }

                            p += end_idx;
                            break;
                        }
                    }

                    p++;
                    curr_line++;
                    curr_idx = 1;
                    carry = CARRY_NONE;
                    n_single_comments++;
                    goto continue_outer;
                }
                else if ((w & multi_comment_bmask) == MULTILINE_COMMENT_START)
                {
                    p = x + 2;
                    curr_idx += idx + 2;
                    i32 comm_start_line = curr_line;
                    i32 comm_start_idx = curr_idx - 2;
                    char const* commstart = p - 2;

                    for (;; p += sizeof(__m256i))
                    {
                        if (UNLIKELY(p >= string_end))
                        {
                            carry = CARRY_NONE;

                            p = commstart;
                            curr_line = comm_start_line;
                            curr_idx = comm_start_idx;

                            goto finalize;
                        }

                        __m256i comment_end = _mm256_set1_epi16(MULTILINE_COMMENT_END);
                        __m256i cb_1 = _mm256_loadu_si256((__m256i const*) p);
                        __m256i cb_2 = _mm256_loadu_si256((__m256i const*) (p + 1));
                        __m256i cb_end_1 = _mm256_cmpeq_epi16(cb_1, comment_end);
                        __m256i cb_end_2 = _mm256_cmpeq_epi16(cb_2, comment_end);
                        __m256i cb_nl = _mm256_cmpeq_epi8(cb_1, cmpmask_newline);
                        u32 cb_mm_1 = _mm256_movemask_epi8(cb_end_1);
                        u32 cb_mm_2 = _mm256_movemask_epi8(cb_end_2);
                        u32 cb_nl_mm = _mm256_movemask_epi8(cb_nl);
                        u32 m1 = cb_mm_1 & 0xAAAAAAAA; /* 10101010... */
                        u32 m2 = cb_mm_2 & 0x55555555; /* 01010101... */
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
                            i32 adv = tz + 1 + ((((u32)1 << tz) & (m2 << 1)) != 0);
                            u32 adv_pow = (u32)1 << tz;
                            cb_nl_mm &= adv_pow | (adv_pow - 1);
                            if (cb_nl_mm)
                            {
                                i32 nladv = 32 - clz32(cb_nl_mm);
                                ASSERT(nladv < adv);
                                curr_line += popcnt(cb_nl_mm);
                                curr_idx = 1 + (adv - nladv);
                            }
                            else
                            {
                                curr_idx += adv;
                            }

                            p += adv;
                            break;
                        }

                        if (cb_nl_mm)
                        {
                            curr_line += popcnt(cb_nl_mm);
                            curr_idx = clz32(cb_nl_mm) + 1;
                        }
                        else
                        {
                            curr_idx += 32;
                        }
                    }

                    carry = CARRY_NONE;
                    n_long_comments++;
                    goto continue_outer;
                }
                else if (size_ge_2)
                {
                    if (0 ALLOWED_TOK3)
                    {
                        CALL_USER(x, 3, T_OP, curr_line, curr_idx + idx, user);

                        u64 skip_idx = 2;
                        u64 skip_mask = ((u64)1 << (skip_idx + 1)) - 2;
                        if (idx + skip_idx >= 64)
                        {
                            p += idx + skip_idx + 1;
                            curr_idx += idx + skip_idx + 1;
                            goto continue_outer;
                        }

                        s &= (~(skip_mask << idx));
                        continue;
                    }
                    else if (0 ALLOWED_TOK2)
                    {
                        CALL_USER(x, 2, T_OP, curr_line, curr_idx + idx, user);

                        u64 skip_idx = 1;
                        u64 skip_mask = ((u64)1 << (skip_idx + 1)) - 2;
                        if (idx + skip_idx >= 64)
                        {
                            p += idx + skip_idx + 1;
                            curr_idx += idx + skip_idx + 1;
                            goto continue_outer;
                        }

                        s &= (~(skip_mask << idx));
                        continue;
                    }
                }

                CALL_USER(x, 1, T_OP, curr_line, curr_idx + idx, user);
            }
        }

        p += 64;
        curr_idx += 64;
    }

finalize:
    {
        /* Update state: */
        lex_impl_result retval;
        retval.out_at = p;
        retval.err = error;
        retval.curr_line = curr_line;
        retval.curr_idx = curr_idx;
        retval.carry = carry;
        retval.carry_tok_len = carry_tok_len;

        return retval;
    }

err_bad_character:
    error = ERR_BAD_CHARACTER;
    goto finalize;

err_bad_char_literal:
    error = ERR_BAD_CHAR_LITERAL;
    goto finalize;

err_newline_in_string:
    error = ERR_NEWLINE_IN_STRING;
    goto finalize;

err_user:
    error = ERR_USER;
    goto finalize;
}
#endif /* __AVX2__ */

lex_result
lex_small(char const* string, char const* string_end,
          int carry, i32 line, i32 idx, i32 carry_tok_len,
          void* user)
{
    u32 single_comment_bmask = TOK_BYTEMASK(SINGLELINE_COMMENT_START);
    u32 multi_comment_bmask = TOK_BYTEMASK(SINGLELINE_COMMENT_START);
    char const* p = string;
    char const* p_end = string_end;
    int state = IN_NONE;
    int err = OK;
    if (carry == CARRY_IDENT)
    {
        state = IN_IDENT;
        goto in_ident;
    }

    while (p < string_end)
    {
        while (p < string_end && ((*p >= 9 && *p <= 13 && *p != '\n') || *p == ' '))
        {
            p++;
            idx++;
        }

        char c = *p++;
        if (UNLIKELY(c < 9) || (UNLIKELY(c > 13) && UNLIKELY(c < ' ')) || UNLIKELY(c == 127))
        {
            err = ERR_BAD_CHARACTER;
            goto finalize;
        }
        else if (UNLIKELY(c == '\n'))
        {
            idx = 1;
            line++;
            continue;
        }

        i32 more = 0; /* TODO: Move them up, so that it compiles in c++ */
        carry_tok_len = 0;
        u32 w = c;
        if (p < string_end) w |= (*p << 8);
        if (p + 1 < string_end) w |= (*(p + 1) << 16);
        u32 w2 = w & 0xFFFF;
        u32 w3 = w & 0xFFFFFF;

        if (0 ALLOWED_TOK3)
        {
            CALL_USER(p - 1, 3, T_OP, line, idx, user);
            p += 2;
            idx += 3;
            continue;
        }
        else if (0 ALLOWED_TOK2)
        {
            CALL_USER(p - 1, 2, T_OP, line, idx, user);
            p += 1;
            idx += 2;
            continue;
        }
        else if (c == '"')
        {
            state = IN_STRING;
            idx++;
        }
        else if (c == '\'')
        {
            char x[6] = { 0, 0, 0, 0, 0, 0 }; /* TODO: copypaste from "fast" code */
            int i = 0;
            x[i++] = c;
            for (; i < 6 && p + i < p_end; ++i)
                x[i] = *(p + i - 1);

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
                    skip_idx = 5;
                    if (UNLIKELY(x[2] < '0')
                        || UNLIKELY(x[3] < '0')
                        || UNLIKELY(x[4] < '0')
                        || UNLIKELY(x[2] > '9')
                        || UNLIKELY(x[3] > '9')
                        || UNLIKELY(x[4] > '9')
                        || UNLIKELY(x[5] != '\'')
                        /* Make sure val is < 256: */
                        || UNLIKELY(x[2] >= '2' && x[3] >= '5' && x[4] > '6'))
                    {
                        err = ERR_BAD_CHAR_LITERAL;
                        goto finalize;
                    }
                }
            }
            else if (UNLIKELY(x[2] != '\''))
            {
                err = ERR_BAD_CHAR_LITERAL;
                goto finalize;
            }

            CALL_USER(p, skip_idx - 1, T_CHAR, line, idx, user);
            p += skip_idx;
            idx += skip_idx + 1;
            continue;
        }
        else if ((w & single_comment_bmask) == SINGLELINE_COMMENT_START)
        {
            state = IN_SHORT_COMMENT;
        }
        else if ((w & multi_comment_bmask) == MULTILINE_COMMENT_START)
        {
            state = IN_LONG_COMMENT;
        }
        else if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '_')
        {
            state = IN_IDENT;
            carry_tok_len = 1;
            idx++;
        }
        else
        {
            CALL_USER(p - 1, 1, T_OP, line, idx, user);
            idx++;
            continue;
        }

        /* TODO: Calculate backslashes in all of these! */
        switch (state) {
        case IN_STRING:
        {
            i32 in_string_line = line;
            i32 in_string_idx = idx;
repeat_string_end_seek:
            while (p < p_end && *p != '"')
            {
                if (UNLIKELY(*p == '\n'))
                {
                    if ((*(p - 1) != '\\' && (*(p - 1) != '\r' || *(p - 2) != '\\')))
                    {
                        err = ERR_NEWLINE_IN_STRING;
                        goto finalize;
                    }

                    idx = 0;
                    line++;
                }

                idx++;
                p++;
                more++;
            }

            if (UNLIKELY(p >= p_end))
            {
                err = ERR_EOF_AT_STRING;
                goto finalize;
            }

            ASSERT(*p == '"');
            if (UNLIKELY(*(p - 1) == '\\') || UNLIKELY(*(p - 2) == '\\'))
            {
                int bslashes = count_backslashes(p - 1);
                if (bslashes & 1)
                {
                    more++;
                    idx++;
                    p++;
                    goto repeat_string_end_seek;
                }
            }

            idx++;
            p++;

            CALL_USER(p - more - 1, more,
                      T_DOUBLEQ_STR/* TODO: Support other kinds of strings!*/,
                      in_string_line, in_string_idx, user);
        } break;
        case IN_SHORT_COMMENT:
        {
            while (p < p_end
                   && (*p != '\n'
                       || *(p - 1) == '\\'
                       || (*(p - 1) == '\r' && *(p - 2) == '\\')))
            {
                if (UNLIKELY(*p == '\n'))
                {
                    line++;
                    idx = 1;
                }

                idx++;
                p++;
            }

            if (UNLIKELY(p >= p_end))
            {
                idx++;
                err = ERR_EOF_AT_COMMENT;
                goto finalize;
            }

            line++;
            idx = 1;
            p++;
            n_single_comments++;
        } break;
        case IN_LONG_COMMENT:
        {
            idx++;
            i32 nbytes = TOK_NBYTES(MULTILINE_COMMENT_END);
            while (p < p_end - nbytes && TOK_ANY(p, nbytes) != MULTILINE_COMMENT_END)
            {
                if (UNLIKELY(*p == '\n'))
                {
                    idx = 0;
                    line++;
                }

                idx++;
                p++;
            }

            if (UNLIKELY(p >= p_end - nbytes))
            {
                for (; p < p_end; ++p)
                {
                    if (*p == '\n')
                    {
                        line++;
                        idx = 0;
                    }

                    idx++;
                }

                err = ERR_EOF_AT_COMMENT;
                goto finalize;
            }

            idx += 2;
            p += 2;
            n_long_comments++;
        } break;
        case IN_IDENT:
in_ident:
        {
            while (p < p_end
                   && (('a' <= *p && *p <= 'z')
                       || ('A' <= *p && *p <= 'Z')
                       || ('0' <= *p && *p <= '9')
                       || *p == '_'))
            {
                carry_tok_len++;
                idx++;
                p++;
            }

            char tok_start = *(p - carry_tok_len);
            i32 tok_type = ('0' <= tok_start && tok_start <= '9') ? T_INTEGER : T_IDENT;
            if (tok_type == T_INTEGER && UNLIKELY(*p == '.'))
            {
                p++;
                idx++;
                carry_tok_len++;
                for (; p < p_end && (('0' <= *p && *p <= '9') || ('a' <= *p && *p <= 'z') || ('A' <= *p && *p <= 'Z') || *p == '.'); carry_tok_len++, idx++, p++)
                {
                    /* NOP */
                }

                CALL_USER(p - carry_tok_len, carry_tok_len, T_FLOAT, line, idx - carry_tok_len, user);
                carry = CARRY_NONE;
            }
            else
            {
                CALL_USER(p - carry_tok_len, carry_tok_len, tok_type,
                          line, idx - carry_tok_len, user);
            }
        } break;
        default: NOTREACHED;
        }
    }

finalize:
    {
        lex_result retval;
        retval.err = err;
        retval.curr_line = line;
        retval.curr_idx = idx;

        return retval;
    }

err_user:
    err = ERR_USER;
    goto finalize;
}

lex_result
lex(char const* string, isize len, void* user)
{
    i32 err = OK;
    i32 curr_line = 1;
    i32 curr_idx = 1;
    i32 carry = CARRY_NONE;
    i32 carry_tok_len = 0;

    char const* p = string;
#ifdef __AVX2__
    if (LIKELY(len > 64))
    {
        lex_impl_result r = lex_s(string, string + len - 64, user);
        p = r.out_at;
        curr_line = r.curr_line;
        curr_idx = r.curr_idx;
        carry = r.carry;
        carry_tok_len = r.carry_tok_len;

        if (UNLIKELY(r.err != OK))
        {
            err = r.err;
            goto finalize;
        }
    }
#endif /* __AVX2__ */

    if (LIKELY(len > 0))
    {
#if PRINT_LINES
        printf("----------------------------------------------------------------\n");
#endif
        return lex_small(p, string + len, carry, curr_line, curr_idx, carry_tok_len, user);
    }

finalize:
    {
        lex_result retval;
        retval.err = err;
        retval.curr_line = curr_line;
        retval.curr_idx = curr_idx;

        return retval;
    }
}

#endif /* SLEX_H_ */
