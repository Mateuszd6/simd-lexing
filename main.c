extern char const* __asan_default_options(void);
extern char const* __asan_default_options() { return "detect_leaks=0"; }

#include <stdio.h>

#include <pmmintrin.h>
#include <immintrin.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "util.h"

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

    for (;;)
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

        // TODO: Maybe don't use toks_mask, just use idents mask and entire "> '
        // '" mask, then if something is not a part ident it's the ohter token.
        u32 toks_mmask = (u32) _mm256_movemask_epi8(toks_mask);
        u32 idents_m1 = (u32) _mm256_movemask_epi8(nidents_mask1);
        u32 idents_m2 = (u32) _mm256_movemask_epi8(nidents_mask2);
        u32 idents_mmask = idents_m1 | (idents_m2 >> 1);
        u32 idents_fullmask_rev = ~((u32) _mm256_movemask_epi8(idents_mask));
        u32 common_mmask = toks_mmask | idents_mmask;
        ASSERT((toks_mmask & idents_mmask) == 0);

        u32 s = common_mmask;
        while (s)
        {
            i32 idx = __builtin_ctz(s);
            char* x = p + idx;
            s = (s & (s - 1));

            if (('a' <= x[0] && x[0] <= 'z')
                || ('A' <= x[0] && x[0] <= 'Z')
                || x[0] == '_'
                || ('0' <= x[0] && x[0] <= '9')) // TODO: digit
            {
                i32 wsidx; /* TODO: no branch? */
                if (idx < 31)
                {
                    u32 mask = idents_fullmask_rev & (~(((u32) 1 << (idx + 1)) - 1));
                    wsidx = mask ? __builtin_ctz(mask) : 32;
                }
                else
                    wsidx = 32;
                /* asm volatile (""::"g"(&wsidx):"memory"); TODO */

                printf("TOK (L = %d): \"", wsidx - idx);
                printf("%.*s", wsidx - idx, x);
                printf("\"\n");
            }
            else
            {
                /* asm volatile (""::"g"(&x[0]):"memory"); */
                printf("TOK: \"%c\"\n", x[0]);
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
#elif 1
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
    }

#if 0
    fclose(f);
#else
#endif
    return 0;
}
