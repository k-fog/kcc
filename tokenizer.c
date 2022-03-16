#include "kcc.h"

// 現在着目しているトークン
Token *token;

void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int pos = loc - user_input;
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, " ");
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

bool is_alnum(char c) {
    return ('a' <= c && c <= 'z') ||
           ('A' <= c && c <= 'Z') ||
           ('0' <= c && c <= '9') ||
           (c == '_');
}

// 次のトークンが期待した記号or識別子の場合、トークンを1つ読み進めて真を返す
// それ以外の場合、偽を返す
bool consume(char *op) {
    if((token->kind != TK_PUNCT && token->kind != TK_IDENT) ||
       strlen(op) != token-> len ||
       memcmp(token->str, op, token->len)) {
        return false;
    }
    token = token->next;
    return true;
}

// 次のトークンが期待したタイプの場合、真を返す
// それ以外の場合、偽を返す
bool token_is(TokenKind tk) {
    return token->kind == tk;
}

// 次のトークンが期待した記号の場合、トークンを1つ読み進める
// それ以外の場合、エラー
void expect(char *op) {
    if((token->kind != TK_PUNCT && token->kind != TK_IDENT) ||
       strlen(op) != token-> len ||
       memcmp(token->str, op, token->len)) {
        error_at(token->str, "expected \"%s\".", op);
    }
    token = token->next;
}

// 次のトークンが数値の場合、トークンを1つ読み進めてその数値を返す
// それ以外の場合、エラー
int expect_number() {
    if(token->kind != TK_NUM) error_at(token->str, "expect a number.");
    int val = token->val;
    token = token->next;
    return val;
}

// 次のトークンが識別子の場合、トークンを1つ読み進めてその識別子を返す
// それ以外の場合、エラー
Token *expect_ident() {
    if(token->kind != TK_IDENT) error_at(token->str, "expect an identifier.");
    Token *t = token;
    token = token->next;
    return t;
}

char *get_token_str(Token *token) {
    char *name = calloc(1, sizeof(char) * (token->len + 1));
    strncpy(name, token->str, token->len);
    return name;
}

bool at_eof() {
    return token->kind == TK_EOF;
}

// 新しいトークンを作成し、curに繋げる
Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
    Token *t = calloc(1, sizeof(Token));
    t->kind = kind;
    t->str = str;
    t->len = len;
    cur->next = t;
    return t;
}

bool startswith(char *p, char *q) {
    return memcmp(p, q, strlen(q)) == 0;
}

int read_ident(char *p) {
   int len = 0;
   while(is_alnum(*p)) {
       len++;
       p++;
   }
   return len;
}

Token *tokenize(char *p) {
    Token head;
    head.next = NULL;
    Token *cur = &head;

    while(*p) {
        if(isspace(*p)) {
            p++;
            continue;
        }
        if(startswith(p, "==") || startswith(p, "!=") ||
           startswith(p, "<=") || startswith(p, ">=")) {
            cur = new_token(TK_PUNCT, cur, p, 2);
            p += 2;
            continue;
        }
        if(strchr("+-*/()<>=;{},", *p)) {
            cur = new_token(TK_PUNCT, cur, p, 1);
            p++;
            continue;
        }
        if(isdigit(*p)) {
            cur = new_token(TK_NUM, cur, p, 0);
            char *q = p;
            cur->val = strtol(p, &p, 10);
            cur->len = p - q;
            continue;
        }
        int ident_len = read_ident(p);
        if(ident_len != 0) {
            cur = new_token(TK_IDENT, cur, p, ident_len);
            p += ident_len;
            continue;
        }
        error_at(token->str, "it can't be tokenized.");
    }

    cur = new_token(TK_EOF, cur, p, 0);
    return head.next;
}
