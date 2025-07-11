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
    TT_PLUS_EQ,             // +=
    TT_MINUS_EQ,            // -=
    TT_STAR_EQ,             // *=
    TT_SLASH_EQ,            // /=
    TT_EQ_EQ,               // ==
    TT_BANG_EQ,             // !=
    TT_ANGLE_L, TT_ANGLE_R, // < >
    TT_ANGLE_L_EQ,          // <=
    TT_ANGLE_R_EQ,          // >=
    TT_PLUS, TT_MINUS,      // + -
    TT_STAR, TT_SLASH,      // * /
    TT_AMPERSAND,           // &
    TT_BANG,                // !
    TT_PAREN_L, TT_PAREN_R, // ( )
    TT_COMMA,               // ,
    TT_PLUS_PLUS,           // ++
    TT_MINUS_MINUS,         // --
    TT_INT,
    TT_IDENT,
    TT_BRACE_L, TT_BRACE_R, // { }
    TT_SEMICOLON,           // ;
    TT_RETURN,              // return
    TT_IF,                  // if
    TT_ELSE,                // else
    TT_WHILE,               // while
    TT_FOR,                 // for
    TT_EOF,
    META_TT_NUM,
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
typedef struct NodeList NodeList;
typedef struct Var Var;

typedef struct {
    const Token *tokens;
    Token *current_token;
    Node *current_func;
} Parser;

typedef enum {     // token node->???
    NT_INT,        // <integer> integer
    NT_IDENT,      // <identifier> main_token
    NT_ADD,        // + expr
    NT_SUB,        // - expr
    NT_MUL,        // * expr
    NT_DIV,        // / expr
    NT_EQ,         // == expr
    NT_NE,         // != expr
    NT_LT,         // < expr
    NT_LE,         // <= expr
    NT_NEG,        // - unary_expr
    NT_BOOL_NOT,   // ! unary_expr
    NT_ADDR,       // & unary_expr
    NT_DEREF,      // * unary_expr
    NT_ASSIGN,     // = expr
    NT_ASSIGN_ADD, // += expr
    NT_ASSIGN_SUB, // -= expr
    NT_ASSIGN_MUL, // *= expr
    NT_ASSIGN_DIV, // /= expr
    NT_FNCALL,     // <function call> fncall
    NT_BLOCK,      // {<stmt>*} block
    NT_RETURN,     // return unary_expr
    NT_IF,         // if ifstmt
    NT_WHILE,      // while whilestmt
    NT_FOR,        // for forstmt
    NT_FUNC,       // <function> func
} NodeTag;

struct Node {
    NodeTag tag;
    Token *main_token;
    union {
        int integer;
        Node *unary_expr;
        struct { Node *lhs, *rhs; } expr;
        struct { Node *name; NodeList *args; } fncall;
        struct { Node *cond; Node *then; Node *els; } ifstmt;
        struct { Node *cond; Node *body; } whilestmt;
        struct { Node *def; Node *cond; Node *next; Node *body; } forstmt;
        NodeList *block;
        struct { Node *name; NodeList *params; Node *body; Var *locals; } func;
    };
};

struct NodeList {
    Node **nodes;
    int len;
    int capacity;
};

#define DEFAULT_NODELIST_CAP 16
NodeList *nodelist_new(int capacity);
void nodelist_append(NodeList *nlist, Node *node);

struct Var {
    const char *name;
    int len;
    int offset;
    Var *next;
};

Var *var_new(Token *ident, int offset, Var *next);
Var *find_local_var(Var *env, Token *ident);

Parser *parser_new(Token *tokens);
NodeList *parse(Parser *parser);

// codegen
void print_token(Token *token);
void gen(NodeList *nlist);
