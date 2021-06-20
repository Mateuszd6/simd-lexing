extern char const* __asan_default_options(void);
extern char const* __asan_default_options() { return "detect_leaks=0"; }

#include <stdio.h>

#include <pmmintrin.h>
#include <immintrin.h>

#include "util.h"

typedef __m256i lexbuf;
const int nlex = sizeof(lexbuf);

int
main(int argc, char** argv)
{
    if (argc < 2) fatal("Need a file");
    char* fname = argv[1];

    FILE* f = fopen(fname, "rb");
    fseek(f, 0, SEEK_END);
    isize fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* string = calloc(fsize + nlex/* ROUND UP TO SSE BUF SIZE, TODO */, sizeof(char));
    char* p = string;
    fread(string, 1, fsize, f);

    for (;;)
    {
        lexbuf b = _mm256_loadu_si256((lexbuf*) p);
        lexbuf pound = _mm256_set1_epi8('#');
        u32 pound_found = _mm256_movemask_epi8(_mm256_cmpeq_epi8(b, pound));

        {
            printf("|");
            for (int i = 0; i < nlex; ++i)
            {
                if (p[i] == '\n') printf(" ");
                else if (!p[i]) printf(" ");
                else printf("%c", p[i]);
            }
            printf("|\n");
        }

        {
            printf("|");
            for (int i = 0; i < nlex; ++i)
            {
                if (p[i] >= 'a' && p[i] <= 'z') printf("%c", p[i]);
                else if (p[i] >= 'A' && p[i] <= 'Z') printf("%c", p[i]);
                else if (p[i] >= '0' && p[i] <= '9') printf("%c", p[i]);
                else if (p[i] == '_') printf("%c", p[i]);
                else printf(" ");
            }
            printf("|\n");
        }

        {
            printf("|");
            for (int i = 0; i < nlex; ++i)
            {
                // TODO: This does not handle '@' and `
                if (p[i] >= '!' && p[i] <= '/') printf("%c", p[i]);
                else if (p[i] >= ':' && p[i] <= '?') printf("%c", p[i]);
                else if (p[i] >= '[' && p[i] <= '^') printf("%c", p[i]);
                else if (p[i] >= '{' && p[i] <= '~') printf("%c", p[i]);
                else printf(" ");
            }
            printf("|\n");
        }

#if 0
        if (UNLIKELY(pound_found))
        {
            i32 lz = __builtin_ctz(pound_found);
            u32 mask = ((1ULL << (32 - lz)) - 1) << lz;
#if 0
            printf("|");
            for (int i = 0; i < nlex; ++i)
                printf("%s", mask & (1 << i) ? "+" : " ");
            printf("|\n");

            printf("shiftback = %d\n", lz);
#endif
#define BIT(N) ((mask & (1 << N)) ? 0 : 0xFF)
            __m256i masked = _mm256_set_epi8(
                BIT(31), BIT(30), BIT(29), BIT(28),
                BIT(27), BIT(26), BIT(25), BIT(24),
                BIT(23), BIT(22), BIT(21), BIT(20),
                BIT(19), BIT(18), BIT(17), BIT(16),
                BIT(15), BIT(14), BIT(13), BIT(12),
                BIT(11), BIT(10), BIT( 9), BIT( 8),
                BIT( 7), BIT( 6), BIT( 5), BIT( 4),
                BIT( 3), BIT( 2), BIT( 1), BIT( 0));

            __m256i cleared = _mm256_and_si256(b, masked);

            char temp[32];
            _mm256_storeu_si256((void*) temp, cleared);
            {
                printf("|");
                for (int i = 0; i < nlex; ++i)
                {
                    if (temp[i] == '\n') printf(" ");
                    else if (!temp[i]) printf(" ");
                    else printf("%c", temp[i]);
                }
                printf("|\n");
            }
        }
#endif

        p += nlex;

        if (p - string >= fsize) break;
        printf("\n");
        /* if (_mm_movemask_epi8(b)) break; */
    }

    fclose(f);
    // __m128i _mm_lddqu_si128 (__m128i const* mem_addr)
    return 0;
}
