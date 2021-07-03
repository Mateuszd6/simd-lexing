extern char const* __asan_default_options(void);
extern char const* __asan_default_options() { return "detect_leaks=0"; }

#include <stdio.h>

#include <pmmintrin.h>
#include <immintrin.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "util.h"

// #undef ASSERT
// #define ASSERT(...)
// #define printf(...) ((void) 0)
// #define NOOPTIMIZE(EXPR) asm volatile (""::"g"(&EXPR):"memory");
// #define PRINT_LINES 0
#define NOOPTIMIZE(EXPR) ((void) 0)
#define PRINT_LINES 1

typedef __m256i lexbuf;
const int nlex = sizeof(lexbuf);

static inline __m256i
mm_ext_shl8_si256(__m256i a)
{
    /* TODO: This is probably slow, figure out better way! */
    u64 carry = (u64) ((u8) _mm256_extract_epi8(a, 15));
    __m256i shifted = _mm256_bslli_epi128(a, 1);
    __m256i andcarry = _mm256_set_epi64x(0, carry, 0, 0);
    return _mm256_or_si256(shifted, andcarry);
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
    const char * file_name = argv[1];
    int fd = open(argv[1], O_RDONLY);

    /* Get the size of the file. */
    int status = fstat(fd, &st);
    fsize = st.st_size;

    string = (char *) mmap(0, fsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    for (int i = fsize - nlex; i < fsize; ++i)
        string[i] = 0;
    fsize -= nlex;
    char* p = string;
#endif

    i64 curr_line = 1;
    i64 curr_inline_idx = 1;
    for (;;)
    continue_outer:
    {
        lexbuf b = _mm256_loadu_si256((lexbuf*) p);
        lexbuf charmask = _mm256_cmpgt_epi8(b, _mm256_set1_epi8(0x20));
        lexbuf mask1 = _mm256_and_si256(
            _mm256_cmpgt_epi8(b, _mm256_set1_epi8('0' - 1)),
            _mm256_cmpgt_epi8(_mm256_set1_epi8('9' + 1), b));
        lexbuf mask2 = _mm256_and_si256(
            _mm256_cmpgt_epi8(b, _mm256_set1_epi8('a' - 1)),
            _mm256_cmpgt_epi8(_mm256_set1_epi8('z' + 1), b));
        lexbuf mask3 = _mm256_and_si256(
            _mm256_cmpgt_epi8(b, _mm256_set1_epi8('A' - 1)),
            _mm256_cmpgt_epi8(_mm256_set1_epi8('Z' + 1), b));
        lexbuf mask4 = _mm256_cmpeq_epi8(b, _mm256_set1_epi8('_'));

        lexbuf ident_start = _mm256_set1_epi16(0xFF00);

        lexbuf idents_mask = _mm256_or_si256(
            _mm256_or_si256(mask1, mask2),
            _mm256_or_si256(mask3, mask4));
        lexbuf toks_mask = _mm256_andnot_si256(idents_mask, charmask);

        lexbuf idents_mask_shed = mm_ext_shl8_si256(idents_mask);
        lexbuf idents_startmask1 = _mm256_cmpeq_epi16(idents_mask, ident_start);
        lexbuf idents_startmask2 = _mm256_cmpeq_epi16(idents_mask_shed, ident_start);

        lexbuf nidents_mask1 = _mm256_and_si256(idents_mask, idents_startmask1);
        lexbuf nidents_mask2 = _mm256_and_si256(idents_mask_shed, idents_startmask2);

        lexbuf newline = _mm256_cmpeq_epi8(b, _mm256_set1_epi8('\n'));
        u32 newline_mmask = (u32) _mm256_movemask_epi8(newline);

        // TODO: Maybe don't use toks_mask, just use idents mask and entire "> '
        // '" mask, then if something is not a part ident it's the ohter token.
        u32 toks_mmask = (u32) _mm256_movemask_epi8(toks_mask);
        u32 idents_m1 = (u32) _mm256_movemask_epi8(nidents_mask1);
        u32 idents_m2 = (u32) _mm256_movemask_epi8(nidents_mask2);
        u32 idents_mmask = idents_m1 | (idents_m2 >> 1);
        u32 idents_fullmask_rev = ~((u32) _mm256_movemask_epi8(idents_mask));
        u32 common_mmask = toks_mmask | idents_mmask | newline_mmask;
        ASSERT((toks_mmask & idents_mmask) == 0);

        u32 s = common_mmask;
        while (s)
        {
            i32 idx = __builtin_ctz(s);
            char* x = p + idx;
            s = (s & (s - 1));
            int size_ge_2 = s && (s & ((u32)1 << (idx + 1))); /* TODO: Overflow shift */
            int size_ge_3 = size_ge_2 && (s & ((u32)1 << (idx + 2))); /* TODO: Overflow shift */

            if (newline_mmask & ((u32)1 << idx)) // TODO: Unlikely? Perhaps not so much?
            {
                curr_line++;
                curr_inline_idx = -idx;

                continue;
            }

            if (idents_mmask & ((u32)1 << idx)) // TODO: digit
            {
                ASSERT(('a' <= x[0] && x[0] <= 'z')
                       || ('A' <= x[0] && x[0] <= 'Z')
                       || x[0] == '_'
                       || ('0' <= x[0] && x[0] <= '9'));

                i32 wsidx; /* TODO: no branch? */
                if (idx < 31)
                {
                    u32 mask = idents_fullmask_rev & (~(((u32) 1 << (idx + 1)) - 1));
                    wsidx = mask ? __builtin_ctz(mask) : 32;
                }
                else
                    wsidx = 32;
                NOOPTIMIZE(wsidx); /* TODO */

                printf("%s:%ld:%ld: TOK (L = %d): \"",
                       "./test.c", curr_line, curr_inline_idx + idx, wsidx - idx);
                printf("%.*s", wsidx - idx, x);
                printf("\"\n");
            }
            else if (x[0] == '"')
            {
                printf("%s:%ld:%ld: TOK: STRING START\n",
                       "./test.c", curr_line, curr_inline_idx + idx);
                char* save = x++;
                while (*x != '"') /* TODO: Incorrect! */
                    x++;
                curr_inline_idx += idx + (x - save) + 1;
                p = x + 1;
                goto continue_outer;
            }
            else if (size_ge_2 && x[0] == '/' && size_ge_2 && x[1] == '/')
            {
                // No need to calculate curr_inline_idx, it ends with a \n

                // TODO: why it even works?
                printf("Skip // comment\n");
                x++;
                while (*x != '\n') /* TODO: Incorrect! */
                    x++;

                p = x + 1;
                curr_line++;
                curr_inline_idx = 0;
                goto continue_outer;
            }
            else if (size_ge_2 && x[0] == '/' && size_ge_2 && x[1] == '*')
            {
                printf("Skip /* comment\n");
                x++;
                curr_inline_idx += idx + 1;
                while (*x != '*' || *(x + 1) != '/') /* TODO: Incorrect! */
                {
                    if (*x == '\n')
                    {
                        curr_line++;
                        curr_inline_idx = 0;
                    }
                    x++;
                    curr_inline_idx++;
                }
                curr_inline_idx += 2;
                p = x + 2;
                goto continue_outer;
            }
            else
            {
#define TOK2(X, Y) (((u16) X) | ((u16) Y) << 8)

                /* This may give false positives */
                if (size_ge_2
                    && (x[0] == x[1] || x[1] == '=' || (x[1] & 0b11111011) == ':'))
                {
                    u16 w = *((u16*) x); /* TODO: Portable unaligned load */
                    if (size_ge_3
                        && (x[2] == '=')
                        && ((w == TOK2('<', '<')) || (w == TOK2('>', '>'))))
                    {
                        printf("%s:%ld:%ld: TOK3: \"%.*s\"\n",
                               "./test.c", curr_line, curr_inline_idx + idx, 3, x);
                        s ^= ((u32)1 << (idx + 1)) | ((u32)1 << (idx + 2));
                        continue;
                    }
                    else if ((   w == TOK2('+', '+'))
                             || (w == TOK2('-', '-'))
                             || (w == TOK2('-', '>'))
                             || (w == TOK2('<', '<'))
                             || (w == TOK2('>', '>'))
                             || (w == TOK2('&', '&'))
                             || (w == TOK2('|', '|'))
                             || (w == TOK2('<', '='))
                             || (w == TOK2('>', '='))
                             || (w == TOK2('=', '='))
                             || (w == TOK2('!', '='))
                             || (w == TOK2('+', '='))
                             || (w == TOK2('-', '='))
                             || (w == TOK2('*', '='))
                             || (w == TOK2('/', '='))
                             || (w == TOK2('%', '='))
                             || (w == TOK2('&', '='))
                             || (w == TOK2('^', '='))
                             || (w == TOK2('|', '='))
                             || (w == TOK2('?', ':')))
                    {
                        // TODO: This should match */, because if we find this token
                        // here it means we should end parsing with a lexing error
                        printf("%s:%ld:%ld: TOK2: \"%.*s\"\n",
                               "./test.c", curr_line, curr_inline_idx + idx, 2, x);
                        s ^= ((u32)1 << (idx + 1));
                        continue;
                    }
                }

                NOOPTIMIZE(x[0]);
                printf("%s:%ld:%ld: TOK: \"%c\"\n", "./test.c", curr_line, curr_inline_idx + idx, x[0]); /* TODO: idx + 1 > 32 */
            }

            /* printf("TOK: \""); */
            /* for (int i = 0; i < 8; ++i) */
            /* printf("%c", x[i] == '\n' ? ' ' : x[i]); */
            /* printf("\"\n"); */
        }

        {
#if 0
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
#elif PRINT_LINES
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
            /* printf("|");
               for (int i = 0; i < nlex; ++i)
               {
               if (_mm256_movemask_epi8(ident_start1) & (1 << i)) printf("F");
               else printf(" ");
               }
               printf("|\n"); */
            printf("|");
            for (int i = 0; i < nlex; ++i)
            {
                if (common_mmask & (1 << i)) printf("*");
                else printf(" ");
            }
            printf("|\n");
            /*
              printf("|");
              for (int i = 0; i < nlex; ++i)
              {
              if (_mm256_movemask_epi8(idents_startmask2) & (1 << i)) printf("F");
              else printf(" ");
              }
              printf("|\n"); */
            printf("\n");
#elif 0
            static int q = 0;
            if (!q) printf("|");
            for (int i = 0; i < nlex; ++i)
            {
                if (p[i] == '\n') printf(" ");
                else if (!p[i]) printf(" ");
                else printf("%c", p[i]);
            }

            if (q) printf("|\n");
            q = 1 - q;
#elif 0
            printf("|");
            for (int i = 0; i < 16; ++i)
            {
                if (p[i] == '\n') printf(" ");
                else if (!p[i]) printf(" ");
                else printf("%c", p[i]);
            }
            printf("|\n");
            printf("|");
            for (int i = 16; i < 32; ++i)
            {
                if (p[i] == '\n') printf(" ");
                else if (!p[i]) printf(" ");
                else printf("%c", p[i]);
            }
            printf("|\n");
#else
#endif
        }

        p += nlex;

        if (p - string >= fsize) break;
        /* printf("\n"); */
        /* if (_mm_movemask_epi8(b)) break; */
        curr_inline_idx += 32; /* TODO: lexbuf size */
    }

#if 0
    fclose(f);
#else
#endif
    return 0;
}
