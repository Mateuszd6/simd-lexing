extern char const* __asan_default_options(void);
extern char const* __asan_default_options() { return "detect_leaks=0"; }

#include <stdio.h>

#include <pmmintrin.h>
#include <immintrin.h>

#include "util.h"

typedef __m128i lexbuf;
const int nlex = 32;

int
main(int argc, char** argv)
{
    if (argc < 2) fatal("Need a file");
    char* fname = argv[1];

    FILE* f = fopen(fname, "rb");
    fseek(f, 0, SEEK_END);
    isize fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* string = calloc(fsize + nlex/* ROUDN UP TO SSE BUF SIZE, TODO */, sizeof(char));
    char* p = string;
    fread(string, 1, fsize, f);

    for (;;)
    {
        lexbuf b = _mm_loadu_si128((lexbuf*) p);
        /* lexbuf hash = _mm256_set1_epi8('#'); */

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

        p += nlex;

        if (p - string >= fsize) break;
        printf("\n");
        /* if (_mm_movemask_epi8(b)) break; */
    }

    fclose(f);
    // __m128i _mm_lddqu_si128 (__m128i const* mem_addr)
    return 0;
}
