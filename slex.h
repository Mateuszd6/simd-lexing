/* TODO: 64 and 32 are hardcoded everywhere
 * TODO: CHeck for stray characters; < 9(HT) or > 13(CR) or 127 (DEL)
 * TODO: Treat characters with first bit set as valid parts of identfier (utf8)
 * TODO: Lexing floats properly
 * TODO: Prefix the API!
 */

#ifndef SLEX_H_
#define SLEX_H_

/* Define ON_TOKEN_CB _before_ including this file, to a function that is called
 * on each token. For performance reasons, the function should be a static
 * inline and be defined in a same translation unit (that's the point, an API
 * with a function pointer is slower, even if function is static and defined in
 * the same file!). Callback should have a signature:

 * void func(char const* str, i32 len, i32 type, i32 line, i32 idx, void* user)
 * where:
 *   TODO: Verify this docs
 *   len is a length of a string (if token is either ident, integer or string)
 *   type is a token type enum
 *   line and idx are line and column number
 *   user is an arbitrary user data that gets passed in
 */

#include <string.h> /* memset, memcpy */
#include <stddef.h> /* size_t, ptrdiff_t */
#include <stdint.h> /* (u)int(8,16,32,64) */

/* TODO: Delete them */
static int64_t n_single_comments;
static int64_t n_long_comments;

#ifdef _MSC_VER
#  include <intrin.h>
#endif
#include <immintrin.h>

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
#  define clz32(V) (31 - msvc_clz32((uint32_t) V))
#  define clz64(V) (63 - msvc_clz64((uint64_t) V))
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
msvc_clz32(uint32_t mask)
{
    unsigned long index = 0;
    _BitScanReverse(&index, mask);
    return (int32_t) index;
}
__forceinline static int32_t
msvc_clz64(uint64_t mask)
{
    unsigned long index = 0;
    _BitScanReverse64(&index, mask);
    return (int32_t) index;
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
#  if (defined(__GNUC__)) || (defined(__clang__))
#    define NOOPTIMIZE(EXPR) __asm__ volatile (""::"g"(&EXPR):"memory")
#  else
#    define NOOPTIMIZE(EXPR) (void) (EXPR)
#  endif
#  define PRINT_LINES 0
#else
#  define NOOPTIMIZE(EXPR) ((void) 0)
#  define PRINT_LINES 1
#endif

/* TODO: Remove!! */
#if PRINT_LINES
#  include <stdio.h>
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

enum lex_type
{
    T_IDENT,
    T_OP,
    T_INTEGER,
    T_FLOAT,
    T_CHAR,
    T_DOUBLEQ_STR,
    T_BACKTICK_STR,
};

typedef struct lex_state lex_state;
struct lex_state
{
    char const* string;
    char const* string_end;
    i32 curr_line;
    i32 curr_inline_idx; /* TODO: Rename to curr_idx */
    i32 carry;
    i32 carry_tok_len; /* TODO: Rename + describe */
    i32 out_in;
    i32 in_string_line;
    i32 in_string_idx;
    i32 in_string_parsed;
    char const* out_at;
};

typedef struct lex_result lex_result;
struct lex_result
{
    i32 err;
    i32 curr_line;
    i32 curr_inline_idx;
};

/* TODO: Probably remove? */
typedef union token token;
union token
{
    char* ident;
    char* integer;
    char* str;
    u32 op;
    u32 ch;
    /* TODO: T_FLOAT */
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

static inline i32
count_backslashes(char const* p)
{
    i32 backslashes = 0;
    p -= (*p == '\r' ? 1 : 0); /* Handle CR-LF */
    while (*p-- == '\\')
        backslashes++;

    return backslashes;
}

__attribute__((flatten)) /* TODO: Only for gnu */
static i32
lex_s(lex_state* state, void* user)
{
    char const* p = state->string;
    char const* string_end = state->string_end;
    i32 curr_line = state->curr_line;
    i32 curr_inline_idx = state->curr_inline_idx;
    i32 carry = state->carry;
    i32 carry_tok_len = state->carry_tok_len;
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
        idents_mmask_2 &= ~((u32) ((fixup_mmask_1 & (((u32) 1) << 31)) != 0 ? 1 : 0));

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

        // TODO: Test with a token that spans > 2 buffers!

        /* If previous frame finished with an ident and this one starts with
         * one, it's a continuation of an old ident */
        if (carry == CARRY_IDENT)
        {
            /* TODO: len, tok_type, line, idx must be saved from the prev stuff? */
            if (LIKELY(idents_fullmask_rev != ((u32) 0))) /* It's a revert of ident mask, so we commpare to 0! */
            {
                i32 addidx = 0;
                if (idents_mmask & 1)
                {
                    s = (s & (s - 1)); /* Ignore first token */
                    u64 mask = idents_fullmask_rev & (~1);
                    addidx = mask ? ctz64(mask) : 64; /* TODO, NOTE: To add to the original len */
                }

                char tok_start = *(p - carry_tok_len);
                i32 tok_type = ('0' <= tok_start && tok_start <= '9') ? T_INTEGER : T_IDENT;
                ON_TOKEN_CB(p - carry_tok_len, carry_tok_len + addidx, tok_type,
                            curr_line, curr_inline_idx - carry_tok_len, user);
            }
            else
            {
                /* This is a very unlikely case; we've loaded a buffer, which is
                 * whole a continuation of current token, and is not finished */
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

                /* If ident spans on more than one buffer frame, will be
                 * reported later */
                if (s || carry == CARRY_NONE) // TODO: More like carry == CARRY_IDENT, but it's the same here
                {
                    i32 tok_type = ('0' <= x[0] && x[0] <= '9') ? T_INTEGER : T_IDENT;
                    ON_TOKEN_CB(x, wsidx - idx, tok_type,
                                curr_line, curr_inline_idx + idx, user);
                }
                else
                {
                    /* Delay reporting a token */
                    carry_tok_len = wsidx - idx;
                }
            }
            else if (x[0] == '"') // TODO: probably move down? Does it matter?
            {
                curr_inline_idx += idx + 1; // TODO: try to avoid here?
                i32 str_start_line = curr_line;
                i32 str_start_idx = curr_inline_idx;
                char const* strstart = x++;

                // TODO: Don't increment index in the loop, calculate adv from the start?
                for (;; x += sizeof(__m256i))
                {
                    if (UNLIKELY(x >= string_end))
                    {
                        in = IN_STRING;
                        p = x;
                        carry = CARRY_NONE;

                        /* These are only set in this specific case */
                        state->in_string_line = str_start_line;
                        state->in_string_idx = str_start_idx;
                        state->in_string_parsed = p - strstart;

                        goto finalize;
                    }

                    __m256i nb = _mm256_loadu_si256((__m256i*) x);
                    __m256i nb_n = _mm256_cmpeq_epi8(nb, cmpmask_doublequote);
                    __m256i nl_n = _mm256_cmpeq_epi8(nb, cmpmask_newline);
                    u32 nb_mm = _mm256_movemask_epi8(nb_n);
                    u32 nl_mm = _mm256_movemask_epi8(nl_n);

                    /* TODO: This is incorrect!!! Calculate nl_mm | nb_mm and
                     * handle doublequotes and newlines one-after-another! */


                    int nl_offset = 0;
                    i32 mask = nl_mm | nb_mm;
                    while (mask)
                    {
                        i32 mask_idx = mask ? ctz32(mask) : 32;
                        if (LIKELY(nb_mm & (1 << mask_idx)))
                        {
                            /* Doubleqote, check if backslashed */
                            if (UNLIKELY(x[mask_idx - 1] == '\\') || UNLIKELY(x[mask_idx - 2] == '\\'))
                            {
                                int bslashes = count_backslashes(&x[mask_idx - 1]);
                                if (bslashes & 1)
                                {
                                    mask = mask & (mask - 1);
                                    continue;
                                }
                            }

                            p = x + mask_idx + 1;
                            ON_TOKEN_CB(strstart + 1, p - strstart - 2, T_DOUBLEQ_STR,
                                        str_start_line, str_start_idx, user);
                            curr_inline_idx += mask_idx + 1 - nl_offset;
                            carry = CARRY_NONE;
                            goto continue_outer;
                        }
                        else
                        {
                            /* Newline, check if backslashed correctly */
                            if (LIKELY(x[mask_idx - 1] == '\\') || LIKELY(x[mask_idx - 2] == '\\'))
                            {
                                int bslashes = count_backslashes(&x[mask_idx - 1]);
                                if (bslashes & 1) /* Odd number of backslashes cancels out */
                                {
                                    curr_line++;
                                    curr_inline_idx = 1;
                                    nl_offset = mask_idx + 1;
                                    mask = mask & (mask - 1);
                                    continue;
                                }
                            }

                            curr_inline_idx += mask_idx - nl_offset;
                            goto err_newline_in_string;
                        }
                    }

                    curr_inline_idx += 32 - nl_offset;
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

                ON_TOKEN_CB(x + 1, skip_idx - 1, T_CHAR,
                            curr_line, curr_inline_idx + idx, user);
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
            else
            {
                if (size_ge_2)
                {
                    u32 w = u32_loadu(x);
                    u32 w2 = w & 0xFFFF;
                    u32 w3 = w & 0xFFFFFF;

                    if (w2 == TOK2('/', '/'))
                    {
                        u64 m = s & newline_mmask;
                        if (m) /* Short skip, newline in the same buf */
                        {
                            i32 comment_end_idx = ctz64(m);
                            if (UNLIKELY(p[comment_end_idx - 1] == '\\')
                                || (UNLIKELY(p[comment_end_idx - 2] == '\\')))
                            {
                                /* Don't do fast skip, when we have some creepy
                                 * backslashes next to the newline. Might give
                                 * some false positives though. */
                                goto skip_long;
                            }

                            s &= ~(((u64)1 << comment_end_idx) - 1);
                            n_single_comments++;
                            continue;
                        }

skip_long:
                        p = x + 1;
                        for (;; p += sizeof(__m256i))
                        {
                            if (UNLIKELY(p >= string_end))
                            {
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

                            __m256i cbuf = _mm256_loadu_si256((__m256i*) p);
                            __m256i cbuf_n = _mm256_cmpeq_epi8(cbuf, cmpmask_newline);
                            u32 cbuf_mm = _mm256_movemask_epi8(cbuf_n);

repeat_nl_seek:
                            /* At least with gcc, backslash in multi-line
                             * single-line-comments cannot be escaped */
                            if (cbuf_mm)
                            {
                                i32 end_idx = ctz32(cbuf_mm);
                                if (UNLIKELY(p[end_idx - 1] == '\\')
                                    || UNLIKELY(p[end_idx - 2] == '\\' && p[end_idx - 1] == '\r'))
                                {
                                    curr_line++;
                                    curr_inline_idx = 1; /* TODO: Probably not necesarry */
                                    cbuf_mm = (cbuf_mm & (cbuf_mm - 1));
                                    goto repeat_nl_seek;
                                }

                                p += end_idx;
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
                        p = x + 2;
                        curr_inline_idx += idx + 2;
                        for (;; p += sizeof(__m256i))
                        {
                            if (UNLIKELY(p >= string_end))
                            {
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

                        carry = CARRY_NONE;
                        n_long_comments++;
                        goto continue_outer;
                    }
                    else if (    w3 == TOK3('>', '>', '=')
                             || (w3 == TOK3('<', '<', '='))
                             || (w3 == TOK3('.', '.', '.')))
                    {
                        ON_TOKEN_CB(x, 3, T_OP, curr_line, curr_inline_idx + idx, user);

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
                             || (w2 == TOK2('?', ':'))
                             /* TODO: Also compare with long comment token here;
                                this should match * /, because if we find this
                                token here it means we should end parsing with a
                                lexing error */
                        )
                    {

                        ON_TOKEN_CB(x, 2, T_OP, curr_line, curr_inline_idx + idx, user);

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

                ON_TOKEN_CB(x, 1, T_OP, curr_line, curr_inline_idx + idx, user);
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
    state->carry_tok_len = carry_tok_len;
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
lex(char const* string, isize len, void* user)
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
        err = lex_s(&state, user);
        if (UNLIKELY(err != OK))
            goto finalize;
    }
    else if (len > 0)
    {
        // Fixup state, because len turned out to be too small
        state.string_end = string + len;
        state.out_at = string;
        state.out_in = IN_NONE;
    }
    else
    {
        goto finalize;
    }

    char const* p = state.out_at;
    i32 offset = (i32) (p - state.string_end);
    char string_tail[64 + 64]; /* TODO: Don't hardcode 64! */
    memset(string_tail, ' ', sizeof(string_tail));
    memcpy(string_tail, state.string_end + offset, 64 - offset);
    state.string = string_tail;
    state.string_end = string_tail + 64 - offset;

    if (UNLIKELY(state.out_in != IN_NONE))
    {
        switch (state.out_in) {
        case IN_STRING:
        {
            i32 more = 0;

            /* If previous frame ended with a '\': */
            if (UNLIKELY(*(p - 1) == '\\') || UNLIKELY(*(p - 2) == '\\'))
            {
                if ((*(p - 1) == '\\' && *p == '\n')
                    || (*(p - 1) == '\\' && *p == '\r' && *(p + 1) == '\n')
                    || (*(p - 2) == '\\' && *(p - 1) == '\r' && *p == '\n'))
                {
                    i32 add = 1 + (*p == '\r');
                    more += add;
                    state.string += add;
                    state.curr_inline_idx = 1;
                    state.curr_line++;
                }
            }

            while (state.string < state.string_end && *state.string != '"') /* TODO: Check backqotes correctly! */
            {
                /* TODO: Check for buggy -1 when newline is at the beginning of the buffer: */
                if (UNLIKELY(*state.string == '\n'))
                {
                    if ((*(state.string - 1) != '\\'
                         && (*(state.string - 1) != '\r' || *(state.string - 2) != '\\')))
                    {
                        err = ERR_NEWLINE_IN_STRING;
                        goto finalize;
                    }

                    state.curr_inline_idx = 0;
                    state.curr_line++;
                }

                state.curr_inline_idx++;
                state.string++;
                more++;
            }

            if (UNLIKELY(state.string >= state.string_end))
            {
                err = ERR_EOF_AT_STRING;
                goto finalize;
            }

            state.curr_inline_idx++;
            state.string++;

            ON_TOKEN_CB(p - state.in_string_parsed + 1, state.in_string_parsed + more - 1,
                        T_DOUBLEQ_STR/* TODO: Support other kinds of strings!*/,
                        state.in_string_line, state.in_string_idx, user);
        } break;
        case IN_SHORT_COMMENT:
        {
            /* If previous frame ended with a '\': */
            if (UNLIKELY(*(p - 1) == '\\') || UNLIKELY(*(p - 2) == '\\'))
            {
                if ((*(p - 1) == '\\' && *p == '\n')
                    || (*(p - 1) == '\\' && *p == '\r' && *(p + 1) == '\n')
                    || (*(p - 2) == '\\' && *(p - 1) == '\r' && *p == '\n'))
                {
                    i32 add = 1 + (*p == '\r');
                    state.string += add;
                    state.curr_inline_idx = add;
                    state.curr_line++;
                }
            }

            /* TODO: Check for buggy -1 when newline is at the beginning of the buffer */
            while (state.string < state.string_end
                   && (*state.string != '\n'
                       || *(state.string - 1) == '\\'
                       || (*(state.string - 1) == '\r' && *(state.string - 2) == '\\')))
            {
                if (UNLIKELY(*state.string == '\n'))
                {
                    state.curr_line++;
                    state.curr_inline_idx = 1;
                }

                state.string++;
            }

            if (UNLIKELY(state.string >= state.string_end))
            {
                err = ERR_EOF_AT_COMMENT;
                goto finalize;
            }

            state.curr_line++;
            state.curr_inline_idx = 1;
            state.string++;
            n_single_comments++;
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
            n_long_comments++;
        } break;
        default: NOTREACHED;
        }
    }
    else if (state.carry == CARRY_IDENT)
    {
        i32 more = 0;
        while (state.string < state.string_end
               && (   ('a' <= *state.string && *state.string <= 'z')
                   || ('A' <= *state.string && *state.string <= 'Z')
                   || ('0' <= *state.string && *state.string <= '9')
                   || *state.string == '_')) /* TODO: All other character parts of an ident */
        {
            more++;
            state.curr_inline_idx++;
            state.string++;
        }

        char tok_start = *(p - state.carry_tok_len);
        i32 tok_type = ('0' <= tok_start && tok_start <= '9') ? T_INTEGER : T_IDENT;
        state.carry = CARRY_NONE;
        ON_TOKEN_CB(p - state.carry_tok_len, state.carry_tok_len + more, tok_type,
                    state.curr_line, state.curr_inline_idx - state.carry_tok_len - more,
                    user);
    }

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

    err = lex_s(&state, user);
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

finalize:
    {
        lex_result retval;
        retval.err = err;
        retval.curr_line = state.curr_line;
        retval.curr_inline_idx = state.curr_inline_idx;

        return retval;
    }
}

#endif // SLEX_H_