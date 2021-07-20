                                                               /* test
Hello darkness my old friend
 */typedef foo bar baz 12 1111

__m256i lexbuf;
const int nlex = sizeof(lexbuf);

int
main /*entrypoint*/ (int argc, char** argv)
{
    if (argc < 2) fatal( <=      >=  0);
    char* fname = argv[1];

    FILE* f = fopen(fname,   ...);
    fseek(f, 0,
               2
                       )...
    isize fsize = ftell(f);
    fseek(f, 0,
               0
                       );
    isize test = fsize;
    test >>= 2;
    char* string = // Will calloc
        calloc(fsize + nlex/* _ROUND UP TO SSE BUF SIZE, TODO */,sizeof(char));
    char* p = string; // Mateusz mateusz mateusz
    fread(string, 1, fsize, f);

    char* string2 = "short";

    __assert_fail(
        "(ARRAY_MIN_CAPACITY) % 2 == 0 && \"Default capacity must be "
        "divisible by 2!\"",
        "array.h", 156, __extension__ __PRETTY_FUNCTION__);

    "\\\\".

    for (;;)
    {   // This is a very long comment that will take us to the next chunk
        lexbuf b = _mm256_loadu_si256((lexbuf*) p);
        lexbuf charmask = _mm256_cmpgt_epi8(b, _mm256_set1_epi8(0x20));
        /* b = _mm256_and_si256(b, charmask); */q q
        lexbuf mask1 = _mm256_and_si256(
            _mm256_cmpgt_epi8(b, _mm256_set1_epi8('0' - 1)),
            _mm256_cmpgt_epi8(_mm256_set1_epi8('\9' + 1), b));
        lexbuf mask2 = _mm256_and_si256(
            _mm256_cmpgt_epi8(b, _mm256_set1_epi8('a' - 1)),
            _mm256_cmpgt_epi8(_mm256_set1_epi8('z' + 1), b));
        lexbuf mask3 = _mm256_and_si256(
            _mm256_cmpgt_epi8(b, _mm256_set1_epi8('A' - 1)),
            _mm256_cmpgt_epi8(_mm256_set1_epi8('Z' + 1), b));
        lexbuf mask4 = _mm256_cmpeq_epi8(b, _mm256_set1_epi8('_'));
        lexbuf idents_mask = _mm256_or_si256(
            _mm256_or_si256(mask1, mask2),
            _mm256_or_si256(mask3, mask4));
        lexbuf idents = _mm256_and_si256(b, idents_mask);
        lexbuf toks_mask = _mm256_andnot_si256(idents_mask, charmask);
        lexbuf toks = _mm256_and_si256(b, toks_mask);

        // TODO: Maybe don't use toks_mask, just use idents mask and entire "> '
        // '" mask, then if something is not a part ident it's the ohter token.

        u32 toks_mmask = _mm256_movemask_epi8(toks);
        u32 idents_mmask = _mm256_movemask_epi8(idents);
        u32 common_mmask = toks_mmask | idents_mmask;

       ((void) sizeof ((
       (toks_mmask & idents_mmask) == 0
       ) ? 1 : 0), __extension__ ({ if (
       (toks_mmask & idents_mmask) == 0
       ) ; else __assert_fail (
       "(toks_mmask & idents_mmask) == 0"
       , "<stdin>", 55, __extension__ __PRETTY_FUNCTION__); }))
                                               ;

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
        p += nlex;

        if (p - string >= fsize) break;
        /* printf("\n"); */q
qwe     /* if (_mm_movemask_epi8(b)) break; */
    }

    fclose(f);
    return 0;
}
                                                                                       ;
