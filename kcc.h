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
    TK_RESERVED, // 記号
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
extern bool consume_num();
extern bool consume_ident();
extern void expect(char *op);
extern int expect_number();
extern char *expect_ident();
extern bool at_eof();
extern Token *new_token(TokenKind kind, Token *cur, char *str, int len);
extern bool startswith(char *p, char *q);
extern Token *tokenize(char *p);

/*
 * parser.c
 */
typedef struct Node Node;
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
    ND_LVAR    // local variable
} NodeKind;

// 抽象構文木のノードの型
struct Node {
    NodeKind kind;  // ノードの型
    Node *lhs;      // 左辺(left-hand side)
    Node *rhs;      // 右辺(right-hand side)
    int val;        // ND_NUMの場合、その数値
    int offset;     // ND_LVARの場合、RBPからのオフセット
};


extern Node *new_node(NodeKind kind, Node *lhs, Node *rhs);
extern Node *new_node_num(int val);
extern Node *new_node_lvar(char *str);
extern void program();
extern Node *stmt();
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