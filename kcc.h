#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMPILER_NAME "KCC"
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
    TT_AND_AND,             // &&
    TT_PIPE_PIPE,           // ||
    TT_KW_SIZEOF,           // sizeof
    TT_PAREN_L, TT_PAREN_R, // ( )
    TT_COMMA,               // ,
    TT_PERIOD,              // .
    TT_MINUS_ANGLE_R,       // ->
    TT_PLUS_PLUS,           // ++
    TT_MINUS_MINUS,         // --
    TT_INT,
    TT_IDENT,
    TT_STRING,
    TT_CHAR,
    TT_BRACE_L, TT_BRACE_R, // { }
    TT_BRACKET_L,           // [
    TT_BRACKET_R,           // ]
    TT_COLON,               // :
    TT_SEMICOLON,           // ;
    TT_HASH,                // #
    TT_KW_RETURN,           // return
    TT_KW_IF,               // if
    TT_KW_ELSE,             // else
    TT_KW_WHILE,            // while
    TT_KW_FOR,              // for
    TT_KW_VOID,             // void
    TT_KW_INT,              // int
    TT_KW_CHAR,             // char
    TT_KW_STRUCT,           // struct
    TT_KW_CONST,            // const
    TT_KW_BREAK,            // break
    TT_KW_CONTINUE,         // continue
    TT_KW_DO,               // do
    TT_KW_SWITCH,           // switch
    TT_KW_CASE,             // case
    TT_KW_DEFAULT,          // default
    TT_KW_UNION,            // union
    TT_KW_ENUM,             // enum
    TT_KW_TYPEDEF,          // typedef
    TT_PP_DEFINE,           // define
    TT_PP_INCLUDE,          // include
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

// preprocessor
typedef struct Symbol Symbol;
typedef struct {
    const char *input;
    int pos;
    Symbol *defines;
} Preprocessor;

Preprocessor *preprocessor_new(const char *input, Symbol *defines);
Token *preprocess(Preprocessor *pp);

// parser
typedef struct Node Node;
typedef struct NodeList NodeList;
typedef struct TokenList TokenList;
typedef struct Type Type;
typedef struct Env Env;

typedef struct {
    const Token *tokens;
    Token *current_token;
    Node *current_func;
    Symbol *func_types;
    Symbol *global_vars;
    Symbol *defined_types;
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
    NT_COMMA,      // , expr
    NT_AND,        // && expr
    NT_OR,         // || expr
    NT_DOT,        // . member_access
    NT_ARROW,      // -> member_access
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
    NT_BREAK,      // break
    NT_CONTINUE,   // continue
    NT_DO_WHILE,   // do-while whilestmt
    NT_SWITCH,     // switch switchstmt
    NT_CASE,       // case caseblock
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
        struct { Node *lhs; Node *member; } member_access;
        struct { Node *control; NodeList *cases; } switchstmt;
        struct { Node *constant; NodeList *stmts; } caseblock; // constant == NULL -> default: label
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
    ST_LVAR, ST_GVAR, ST_FUNC, ST_STRUCT, ST_UNION, ST_ENUM, ST_MEMBER, ST_TYPEDEF, ST_DEFINE,
} SymbolTag;

struct Symbol {
    SymbolTag tag;
    Token *token;
    Type *type;
    Symbol *next;

    union {
        int offset; // for local variable, struct
        int value;  // for enum
        Node *init; // for global variable
        Token *pp_token; // for #define macro
    };
};

Symbol *reverse_symbols(Symbol *list);
int align_n(int x, int n);

typedef struct {
    NodeList *funcs;
    Symbol *func_types;
    Symbol *global_vars;
    Symbol *defined_types;
    TokenList *string_tokens;
} Program;

Symbol *find_symbol(SymbolTag tag, Symbol *symlist, Token *ident);
Symbol *find_member(Symbol *symlist, Token *ident, int *offset);
Symbol *find_enum_val(Symbol *defined_types, Token *ident);

Parser *parser_new(Token *tokens);
Program *parse(Parser *parser);

// type
struct Type {
    enum { TYP_VOID, TYP_CHAR, TYP_INT, TYP_PTR, TYP_ARRAY, TYP_STRUCT, TYP_UNION, TYP_ENUM } tag;
    int array_size; // array
    union {
        Type *base; // pointer to
        struct { Token *ident; Symbol *list; int size; int align; } tagged_typ; // struct
    };
};

extern Type *type_void;
extern Type *type_int;
extern Type *type_char;

struct Env {
    Symbol *local_vars;
    Symbol *global_vars;
    Symbol *func_types;
    Symbol *defined_types;
};

bool is_integer(Type *type);
bool is_ptr_or_arr(Type *type);
bool tokeneq(Token *a, Token *b);
Env *env_new(Symbol *local_vars, Symbol *global_vars, Symbol *func_types, Symbol *defined_types);

int sizeof_type(Type *type);
int alignof_type(Type *type);
Type *type_copy(Type *type);
Type *pointer_to(Type *base);
Type *array_of(Type *base, int size);
Type *struct_new(Token *ident, Symbol *list, int size, int align);
Type *union_new(Token *ident, Symbol *list, int size, int align);
Type *enum_new(Token *ident, Symbol *list);
void type_funcs(Program *prog);

// codegen
#define LOOP_STACK_SIZE 16
typedef struct {
    int loop_id_stack[LOOP_STACK_SIZE];
    int id_stack_top;
    Node *current_func;
    Symbol *local_vars;
    Symbol *global_vars;
    Symbol *func_types;
    Symbol *defined_types;
} GenContext;
void print_token(Token *token);
void gen(Program *prog);

// main
char *read_file(char *path);
