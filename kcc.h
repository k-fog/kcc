#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * tokenizer.c
 */
typedef struct Token Token;

// トークンの種類
typedef enum {
    TK_PUNCT,    // 記号
    TK_IDENT,    // 識別子
    TK_NUM,      // 整数
    TK_EOF,      // 入力の終わり
} TokenKind;

// トークン型
struct Token {
    TokenKind kind; // トークンの種類
    Token *next;    // 次のトークン
    int val;        // TK_NUMの場合、その数値
    char *str;      // トークン文字列
    int len;        // トークンの長さ
};

extern Token *token;

extern void error(char *fmt, ...);
extern void error_at(char *loc, char *fmt, ...);
extern bool consume(char *op);
extern bool token_is(TokenKind tk);
extern void expect(char *op);
extern int expect_number();
extern Token *expect_ident();
extern bool at_eof();
extern Token *new_token(TokenKind kind, Token *cur, char *str, int len);
extern Token *tokenize(char *p);

/*
 * parser.c
 */
typedef struct Node Node;
typedef struct LVar LVar;
extern Node *code[];

// 抽象構文木のノードの種類
typedef enum {
    ND_ADD,    // +
    ND_SUB,    // -
    ND_MUL,    // *
    ND_DIV,    // /
    ND_EQ,     // ==
    ND_NE,     // !=
    ND_LT,     // <
    ND_LE,     // <=
    ND_ASSIGN, // =
    ND_NUM,    // number
    ND_LVAR,   // local variable
    ND_RETURN, // return
    ND_IF,     // if
    ND_WHILE,  // while
    ND_FOR,    // for
} NodeKind;

// 抽象構文木のノードの型
struct Node {
    NodeKind kind;  // ノードの型
    Node *lhs;      // 左辺(left-hand side)
    Node *rhs;      // 右辺(right-hand side)

    // if & for & while
    Node *cond;
    Node *then;
    Node *els;
    Node *init;
    Node *inc;

    int val;        // ND_NUMの場合、その数値
    int offset;     // ND_LVARの場合、RBPからのオフセット
};

// ローカル変数
struct LVar {
    LVar *next;  // 次の変数かNULL
    char *name;  // 変数名
    int len;
    int offset;
};

extern LVar *locals;

extern Node *new_node(NodeKind kind, Node *lhs, Node *rhs);
extern Node *new_node_empty(NodeKind kind);
extern Node *new_node_num(int val);
extern Node *new_node_lvar(Token *token);
extern LVar *find_lvar(Token *token);
extern void program();
extern Node *stmt();
extern Node *stmt_if();
extern Node *stmt_for();
extern Node *stmt_while();
extern Node *expr();
extern Node *assign();
extern Node *equality();
extern Node *relational();
extern Node *add();
extern Node *mul();
extern Node *unary();
extern Node *primary();

/*
 * codegen.c
 */
extern void gen(Node *node);


/*
 * main.c
 */
extern char *user_input;
