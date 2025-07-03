#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
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
    TT_EQ,                  // =
    TT_PLUS, TT_MINUS,      // + -
    TT_STAR, TT_SLASH,      // * /
    TT_BANG,                // !
    TT_PAREN_L, TT_PAREN_R, // ( )
    TT_EQ_EQ,               // ==
    TT_BANG_EQ,             // !=
    TT_ANGLE_L, TT_ANGLE_R, // < >
    TT_ANGLE_L_EQ,          // <=
    TT_ANGLE_R_EQ,          // >=
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
    NT_INT,      // <integer> integer
    NT_IDENT,    // <identifier> main_token
    NT_ADD,      // + expr
    NT_SUB,      // - expr
    NT_MUL,      // * expr
    NT_DIV,      // / expr
    NT_EQ,       // == expr
    NT_NE,       // != expr
    NT_LT,       // < expr
    NT_LE,       // <= expr
    NT_NEG,      // - unary_expr
    NT_BOOL_NOT, // ! unary_expr
    NT_ASSIGN,   // = expr
} NodeTag;

struct Node {
    NodeTag tag;
    Token *main_token;
    union {
        int integer;
        Node *unary_expr;
        struct {Node *lhs, *rhs;} expr;
    };
};

Parser *parser_new(Token *tokens);
Node *parse(Parser *parser);

// codegen
void gen(Node *node);
