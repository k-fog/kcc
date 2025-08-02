#include <ctype.h>
#include <errno.h>
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
    TT_PERCENT,             // %
    TT_AMPERSAND,           // &
    TT_BANG,                // !
    TT_QUESTION,            // ?
    TT_KW_SIZEOF,           // sizeof
    TT_PAREN_L, TT_PAREN_R, // ( )
    TT_COMMA,               // ,
    TT_PLUS_PLUS,           // ++
    TT_MINUS_MINUS,         // --
    TT_INT,
    TT_IDENT,
    TT_STRING,
    TT_BRACE_L, TT_BRACE_R, // { }
    TT_BRACKET_L,           // [
    TT_BRACKET_R,           // ]
    TT_COLON,               // :
    TT_SEMICOLON,           // ;
    TT_KW_RETURN,           // return
    TT_KW_IF,               // if
    TT_KW_ELSE,             // else
    TT_KW_WHILE,            // while
    TT_KW_FOR,              // for
    TT_KW_VOID,             // void
    TT_KW_INT,              // int
    TT_KW_CHAR,             // char
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
typedef struct TokenList TokenList;
typedef struct Symbol Symbol;
typedef struct Type Type;
typedef struct Env Env;

typedef struct {
    const Token *tokens;
    Token *current_token;
    Node *current_func;
    Symbol *func_types;
    Symbol *global_vars;
    TokenList *string_tokens;
} Parser;

typedef enum {     // token node->???
    NT_INT,        // <integer> integer
    NT_IDENT,      // <identifier> main_token
    NT_STRING,     // <string> main_token, index: index of string_literals
    NT_ADD,        // + expr
    NT_SUB,        // - expr
    NT_MUL,        // * expr
    NT_DIV,        // / expr
    NT_MOD,        // % expr
    NT_EQ,         // == expr
    NT_NE,         // != expr
    NT_LT,         // < expr
    NT_LE,         // <= expr
    NT_NEG,        // - unary_expr
    NT_BOOL_NOT,   // ! unary_expr
    NT_ADDR,       // & unary_expr
    NT_DEREF,      // * unary_expr
    NT_PREINC,     // ++ unary_expr
    NT_PREDEC,     // -- unary_expr
    NT_POSTINC,    // ++ pre_expr 
    NT_POSTDEC,    // -- pre_expr
    NT_ASSIGN,     // = expr
    NT_ASSIGN_ADD, // += expr
    NT_ASSIGN_SUB, // -= expr
    NT_ASSIGN_MUL, // *= expr
    NT_ASSIGN_DIV, // /= expr
    NT_COND,       // ?: conditional
    NT_FNCALL,     // <function call> fncall
    NT_BLOCK,      // {<stmt>*} block
    NT_RETURN,     // return unary_expr
    NT_IF,         // if ifstmt
    NT_WHILE,      // while whilestmt
    NT_FOR,        // for forstmt
    NT_FUNC,       // <function> func
    NT_DECLARATOR, // <identifier> | <identifier> = <initializer> declarator 
    NT_INITS,      // <initializer list> initializers
    NT_LOCALDECL,  // <local variable declaration> declarators
    NT_PARAMDECL,  // <parameter declaration> ident
    NT_GLOBALDECL, // <global variable declaration> unary_expr
    NT_SIZEOF,     // sizeof unary_expr
    NT_TYPENAME,   // <type> type (for sizeof)
} NodeTag;

struct Node {
    NodeTag tag;
    Token *main_token;
    Type *type;
    union {
        int integer;
        int index;
        Node *unary_expr;
        Node *pre_expr;
        Node *ident;
        struct { Node *lhs, *rhs; } expr;
        struct { Node *name; NodeList *args; } fncall;
        struct { Node *cond; Node *then; Node *els; } ifstmt;
        struct { Node *cond; Node *then; Node *els; } cond_expr;
        struct { Node *cond; Node *body; } whilestmt;
        struct { Node *def; Node *cond; Node *next; Node *body; } forstmt;
        NodeList *block;
        struct { Node *name; NodeList *params; Node *body; Symbol *locals; } func;
        struct { Node *name; Node *init; } declarator;
        NodeList *declarators;
        NodeList *initializers;
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

struct TokenList {
    Token **tokens;
    int len;
    int capacity;
};

#define DEFAULT_TOKENLIST_CAP 16
TokenList *tokenlist_new(int capacity);
void tokenlist_append(TokenList *tlist, Token *token);

typedef enum {
    ST_LVAR, ST_GVAR, ST_FUNC
} SymbolTag;

struct Symbol {
    SymbolTag tag;
    Token *token;
    Type *type;
    Symbol *next;

    union {
        int offset; // for local variable
        Node *init; // for global variable
    };
};

typedef struct {
    NodeList *funcs;
    Symbol *func_types;
    Symbol *global_vars;
    TokenList *string_tokens;
} Program;

Symbol *find_symbol(SymbolTag tag, Symbol *symlist, Token *ident);

Parser *parser_new(Token *tokens);
Program *parse(Parser *parser);

// type
struct Type {
    enum { TYP_VOID, TYP_CHAR, TYP_INT, TYP_PTR, TYP_ARRAY } tag;
    Type *base; // pointer to
    int array_size;
};

extern Type *type_void;
extern Type *type_int;
extern Type *type_char;

struct Env {
    Symbol *local_vars;
    Symbol *global_vars;
    Symbol *func_types;
};

bool is_integer(Type *type);
bool is_ptr_or_arr(Type *type);
Env *env_new(Symbol *local_vars, Symbol *global_vars, Symbol *func_types);

int sizeof_type(Type *type);
Type *type_copy(Type *type);
Type *pointer_to(Type *base);
Type *array_of(Type *base, int size);
void type_funcs(Program *prog);

// codegen
void print_token(Token *token);
void gen(Program *prog);
