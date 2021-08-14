#ifdef __linux__
#  define USE_MMAP 1
#endif

/* TODO: Remove this when done */
#include <assert.h>
#define ASSERT assert

/* TODO: This should be a part of the example file, _before_ including this file */
static inline void
tok_print(char const* str, int len, int type, int line, int idx, void* user);
#define ON_TOKEN_CB tok_print

#include "slex.h"

/* TODO: Get rid of them! */
#if 1
static i64 n_parsed_idents = 0;
static i64 n_parsed_numbers = 0;
static i64 n_parsed_strings = 0;
static i64 n_parsed_chars = 0;
static i64 n_single_comments = 0;
static i64 n_long_comments = 0;
#endif

#if USE_MMAP
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#endif

#ifdef _MSC_VER
#  define _CRT_SECURE_NO_DEPRECATE
#endif
#include <stdio.h>

static inline void
tok_print(char const* str, i32 len, i32 type, i32 line, i32 idx, void* user)
{
    char const* f = (char const*) user;
    (void) f;
    (void) str;
    (void) len;
    (void) type;
    (void) line;
    (void) idx;

    switch(type) {
    case T_IDENT:
    {
        n_parsed_idents++;
    } break;
    case T_INTEGER:
    {
        n_parsed_numbers++;
    } break;
    case T_DOUBLEQ_STR:
    case T_BACKTICK_STR:
    {
        n_parsed_strings++;
    } break;
    case T_CHAR:
    {
        n_parsed_chars++;
    } break;
    /*
    case T_OP:
    {
    } break;
    case T_FLOAT:
    {
    } break;
    */
    default:
    {
    } break;
    }

#if PRINT_TOKENS
    printf("%s:%d:%d: TOK %d (L = %d): \"%.*s\"\n", f, line, idx, type, len, len, str);
#endif
}

int
main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "%s: fatal: Need a file\n", argv[0]);
        exit(1);
    }

    char* fname = argv[1];

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
    fstat(fd, &st); /* Get the size of the file. */ /* TODO: Check retval of stat and fail */
    isize fsize = st.st_size;
    char* string = (char*) mmap(0, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
#endif

    lex_result r = lex(string, fsize, (void*) fname);
    if (r.err != OK)
    {
        fprintf(stderr, "%s:%d:%d: error: %s\n",
                fname, r.curr_line, r.curr_idx, lex_error_str[r.err]);
        exit(1);
    }

    fprintf(stdout, "Parsed: "
            "%d lines, %ld ids, %ld strings, "
            "%ld chars, %ld ints, %ld hex,   %ld floats,    "
            "%ld //s, %ld /**/s,   0 #foo\n",
            r.curr_line, n_parsed_idents, n_parsed_strings,
            n_parsed_chars, n_parsed_numbers, 0L, 0L,
            n_single_comments, n_long_comments);

    return 0;
}
