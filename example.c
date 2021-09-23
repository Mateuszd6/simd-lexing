#ifdef __linux__
#  define USE_MMAP 1
#endif

#include <stdio.h>
#include <stdlib.h>

/* TODO: Remove this when done */
#include <assert.h>
#define ASSERT assert

static inline int
on_token_cb(char const* str, int len, int type, int line, int idx, void* user);

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

#define ON_TOKEN_CB on_token_cb

/* TODO: REMOVE THEM FROM HERE! */
static long n_single_comments = 0;
static long n_long_comments = 0;

#include "slex.h"

#include <stdint.h>
#include <stddef.h>

typedef struct token token;
struct token
{
    union
    {
        char ch_val;
        ptrdiff_t i_val;
        double fp_val;
        int op;
        struct
        {
            char* p;
            ptrdiff_t len;
        } id;
        struct
        {
            char* p;
            ptrdiff_t len;
        } str;
    } u;
    int type;
    i32 line;
    i32 idx;
};

typedef struct lex_data lex_data;
struct lex_data
{
    token* p;
    ptrdiff_t len;
    ptrdiff_t cap;
};

static inline void
lex_data_init(lex_data* data)
{
    data->p = 0;
    data->len = 0;
    data->cap = 0;
}

static inline void
lex_data_push_token(lex_data* data, token t)
{
    if (UNLIKELY(data->len >= data->cap))
    {
        ptrdiff_t new_cap = data->cap * 2;
        if (new_cap < 32)
            new_cap = 32;

        token* new_p = realloc(data->p, sizeof(token) * new_cap);
        data->p = new_p;
        data->cap = new_cap;
    }

    data->p[data->len++] = t;
}

// This can be adjusted so that you have more control over what is parsed and how.
// Returning other value than 0 means an error and lexing is aborted.
static inline int
on_token_cb(char const* str, i32 len, i32 type, i32 line, i32 idx, void* user)
{
    token t;
    t.type = type;
    t.line = line;
    t.idx = idx;

    switch (type) {
    case T_IDENT:
    {
        t.u.id.p = (char*) str;
        t.u.id.len = len;
    } break;
    case T_INTEGER:
    {
        ptrdiff_t parsed_int = strtoll(str, 0, 0);
        t.u.i_val = parsed_int;
    } break;
    case T_DOUBLEQ_STR:
    {
        t.u.str.p = (char*) str;
        t.u.str.len = len;
    } break;
    case T_CHAR:
    {
        if (len == 1)
        {
            t.u.ch_val = (char) str[0];
        }
        else if (len == 2 && str[0] == '\\')
        {
            switch (str[1]) {
            case ('a'): t.u.ch_val = '\a'; break;
            case ('b'): t.u.ch_val = '\b'; break;
            case ('e'): t.u.ch_val = '\e'; break;
            case ('f'): t.u.ch_val = '\f'; break;
            case ('n'): t.u.ch_val = '\n'; break;
            case ('r'): t.u.ch_val = '\r'; break;
            case ('t'): t.u.ch_val = '\t'; break;
            case ('v'): t.u.ch_val = '\v'; break;
            case ('\\'): t.u.ch_val = '\\'; break;
            case ('\''): t.u.ch_val = '\''; break;
            case ('"'): t.u.ch_val = '"'; break;
            default: t.u.ch_val = str[1];
            }
        }
        else if (str[0] == '\\')
        {
            int ret = 0;
            int pw = 1;
            for (int i = 1; i < len; ++i, pw *= 8)
                ret += (str[len - i] - '0') * pw;
            t.u.ch_val = (char)ret;
        }
        else
        {
            return 10; /* User-defined error */
        }
    } break;
    case T_OP:
    {
        union { char bytes[4]; int op; } u;
        memset(u.bytes, 0, 4);
        memcpy(u.bytes, str, len);
        t.u.op = u.op;
    } break;
    case T_FLOAT:
    {
        double parsed_fp = strtod(str, 0);
        t.u.fp_val = parsed_fp;
    } break;
    default:
    {
    } break;
    }

    lex_data_push_token((lex_data*) user, t);
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

    lex_data ld;
    lex_data_init(&ld);

    lex_result r = lex(string, fsize, (void*) &ld);
    if (r.err != OK)
    {
        fprintf(stderr, "%s:%d:%d: error: %s\n",
                fname, r.curr_line, r.curr_idx, lex_error_str[r.err]);
        exit(1);
    }

    //
    // ld.p has ld.len tokens to use. You must free ld.p when done
    // Just a single example:
    //
    for (ptrdiff_t i = 0; i < ld.len; ++i)
    {
        char* type_name = 0;
        switch (ld.p[i].type) {
        case T_IDENT: type_name = "ident"; break;
        case T_OP: type_name = "operator"; break;
        case T_INTEGER: type_name = "integer"; break;
        case T_FLOAT: type_name = "float"; break;
        case T_CHAR: type_name = "char"; break;
        case T_DOUBLEQ_STR: type_name = "string"; break;
        }
        printf("%s:%d:%d: %s\n", fname, ld.p[i].line, ld.p[i].idx, type_name);
    }

    free(ld.p);
    free(string);
    fclose(f);

    return 0;
}
