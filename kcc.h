#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void panic(char *fmt, ...);

// lexer
typedef struct {
    const char *input;
    int pos;
} Lexer;

typedef enum {
    TT_PLUS, TT_MINUS,  // + -
    TT_STAR, TT_SLASH,  // * /
    TT_INT,
    TT_IDENT,
    TT_EOF,
} TokenTag;

typedef struct Token Token;
struct Token {
    TokenTag tag;
    const char *start;
    int len;
    Token *next;
};

Lexer *lexer_new(const char *input);
Token *tokenize(Lexer *lexer);

// parser
typedef struct Node Node;

typedef struct {
    const Token *tokens;
    Token *current_token;
} Parser;

typedef enum {
    NT_INT,  // data->integer
    NT_ADD,  // data->binop
    NT_SUB,  // data->binop
    NT_MUL,  // data->binop
    NT_DIV,  // data->binop
} NodeTag;

struct Node {
    NodeTag tag;
    Token *main_token;
    union {
        int integer;
        struct {Node *lhs, *rhs;} expr;
    };
};

Parser *parser_new(Token *tokens);
Node *parse(Parser *parser);
