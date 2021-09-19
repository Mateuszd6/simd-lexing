#ifdef __linux__
#  define USE_MMAP 1
#endif

#ifdef _MSC_VER
#  define _CRT_SECURE_NO_DEPRECATE
#  define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>

/* TODO: Remove this when done */
#include <assert.h>
#define ASSERT assert

#include "timer.h"

static inline int
tok_print(char const* str, int len, int type, int line, int idx, void* user);

/* Define all things that the lexer needs: */
#define ALLOWED_TOK2                                                           \
    DEF_TOK2('+', '+') DEF_TOK2('-', '-') DEF_TOK2('-', '>')                   \
    DEF_TOK2('<', '<') DEF_TOK2('>', '>') DEF_TOK2('&', '&')                   \
    DEF_TOK2('|', '|') DEF_TOK2('<', '=') DEF_TOK2('>', '=')                   \
    DEF_TOK2('=', '=') DEF_TOK2('!', '=') DEF_TOK2('+', '=')                   \
    DEF_TOK2('-', '=') DEF_TOK2('*', '=') DEF_TOK2('/', '=')                   \
    DEF_TOK2('%', '=') DEF_TOK2('&', '=') DEF_TOK2('^', '=')                   \
    DEF_TOK2('|', '=') DEF_TOK2('?', ':')

#define ALLOWED_TOK3                                                           \
    DEF_TOK3('>', '>', '=')                                                    \
    DEF_TOK3('<', '<', '=')                                                    \
    DEF_TOK3('.', '.', '.')

#define MULTILINE_COMMENT_START TOK2('/', '*')
#define MULTILINE_COMMENT_END TOK2('*', '/')

#define SINGLELINE_COMMENT_START TOK2('/', '/')

#define ON_TOKEN_CB tok_print

/* TODO: Move them below the #include! */
static long n_parsed_idents = 0;
static long n_parsed_numbers = 0;
static long n_parsed_floats = 0;
static long n_parsed_strings = 0;
static long n_parsed_chars = 0;
static long n_single_comments = 0; /* TODO: Remove them at the end? */
static long n_long_comments = 0;

#include "../slex.h"

static inline int
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
    */
    case T_FLOAT:
    {
        n_parsed_floats++;
    } break;
    default:
    {
    } break;
    }

#if PRINT_TOKENS
    printf("%s:%d:%d: TOK %d (L = %d): \"%.*s\"\n", f, line, idx, type, len, len, str);
#endif

    return 0;
}

#if USE_MMAP
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

int
main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "%s: fatal: Need a file\n", argv[0]);
        exit(1);
    }

    char* fname = argv[1];

#ifdef BENCH
#  define REPEAT (256)
    timer t = {0};
    for (int i = 0; i < REPEAT; ++i)
#endif
    {
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

#ifdef BENCH
        TIMER_START(t);
#endif
        lex_result r = lex(string, fsize, (void*) fname);
#ifdef BENCH
        TIMER_STOP(t);
#endif

        if (r.err != OK)
        {
            fprintf(stderr, "%s:%d:%d: error: %s\n",
                    fname, r.curr_line, r.curr_idx, lex_error_str[r.err]);
            exit(1);
        }

#if !(USE_MMAP)
        fclose(f);
#else
        munmap(string, fsize);
        close(fd);
#endif

#ifndef BENCH
        fprintf(stdout, "Parsed: "
                "%d lines, %ld ids, %ld strings, "
                "%ld chars, %ld ints, %ld hex,   %ld floats,    "
                "%ld //s, %ld /**/s,   0 #foo\n",
                r.curr_line, n_parsed_idents, n_parsed_strings,
                n_parsed_chars, n_parsed_numbers, 0L, n_parsed_floats,
                n_single_comments, n_long_comments);
#endif
    }

#ifdef BENCH
    fprintf(stdout, "Avg.: %.3fms\n", (float)t.elapsed / ((long)1000 * 1000 * REPEAT));
#endif
    return 0;
}
