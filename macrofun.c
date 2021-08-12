/* TODO: Don't hardcode w2 and w3 in these macros, because that's bugprone. Now
 * the implementation file should expect them to be defined, and if they are
 * not, maybe define some other macro, that says double and triple ops will
 * never happen, and we can #ifdef out these parts of lexer to speed things up
 */

#define ALLOWED_TOK2                                                           \
    DEF_TOK2('+', '+') DEF_TOK2('-', '-') DEF_TOK2('-', '>')                   \
    DEF_TOK2('<', '<') DEF_TOK2('>', '>') DEF_TOK2('&', '&')                   \
    DEF_TOK2('|', '|') DEF_TOK2('<', '=') DEF_TOK2('>', '=')                   \
    DEF_TOK2('=', '=') DEF_TOK2('!', '=') DEF_TOK2('+', '=')                   \
    DEF_TOK2('-', '=') DEF_TOK2('*', '=') DEF_TOK2('/', '=')                   \
    DEF_TOK2('%', '=') DEF_TOK2('&', '=') DEF_TOK2('^', '=')                   \
    DEF_TOK2('|', '=')

#define ALLOWED_TOK3                                                           \
    DEF_TOK3('>', '>', '=')                                                    \
    DEF_TOK3('<', '<', '=')                                                    \
    DEF_TOK3('.', '.', '.')

#define TOK2(X, Y) (((u32) X) | ((u32) Y) << 8)
#define TOK3(X, Y, Z) (((u32) X) | ((u32) Y) << 8 | ((u32) Z) << 16)

#define DEF_TOK2(X, Y) ||(w2 == TOK2(X, Y))
#define DEF_TOK3(X, Y, Z) ||(w3 == TOK3(X, Y, Z))


0 ALLOWED_TOK2
0 ALLOWED_TOK3
