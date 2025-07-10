#include "kcc.h"

struct {
    char *str; TokenTag tag;
} keywords[] = {
    {"return", TT_RETURN},
    {"if",     TT_IF},
    {"else",   TT_ELSE},
    {"while",  TT_WHILE},
    {"for",    TT_FOR},
    {NULL,     -1},
};

Lexer *lexer_new(const char *input) {
    Lexer *lexer = calloc(1, sizeof(Lexer));
    lexer->input = input;
    lexer->pos = 0;
    return lexer;
}

static Token *token_new(TokenTag tag, const char *start, int len) {
    Token *t = calloc(1, sizeof(Token));
    t->tag = tag;
    t->start = start;
    t->len = len;
    t->next = NULL;
    return t;
}

static char peek(Lexer *lexer) {
    return lexer->input[lexer->pos];
}

static const char *consume(Lexer *lexer) {
    return &(lexer->input[lexer->pos++]);
}

static void skip_space(Lexer *lexer) {
    while (isspace(peek(lexer))) consume(lexer);
}

// if the given token matches a keyword, return its TokenTag.
// otherwise, return TT_IDENT.
static TokenTag lookup_ident(const char *str, int len) {
    for (int i = 0; keywords[i].str != NULL; i++) {
        if (strlen(keywords[i].str) != len) continue;
        else if (strncmp(str, keywords[i].str, len) == 0) return keywords[i].tag;
    }
    return TT_IDENT;
}

Token *tokenize(Lexer *lexer) {
    Token head, *token = &head;
    while (token->tag != TT_EOF) {
        skip_space(lexer);
        const char *start = consume(lexer);
        const char *end = start;

        switch (*start) {
            case '\0':
                token->next = token_new(TT_EOF, start, 1);
                break;
            case '+':
                if (peek(lexer) == '+') {
                    consume(lexer);
                    token->next = token_new(TT_PLUS_PLUS, start, 2);
                } else if (peek(lexer) == '=') {
                    consume(lexer);
                    token->next = token_new(TT_PLUS_EQ, start, 2);
                } else {
                    token->next = token_new(TT_PLUS, start, 1);
                }
                break;
            case '-':
                if (peek(lexer) == '-') {
                    consume(lexer);
                    token->next = token_new(TT_MINUS_MINUS, start, 2);
                } else if (peek(lexer) == '=') {
                    consume(lexer);
                    token->next = token_new(TT_MINUS_EQ, start, 2);
                } else {
                    token->next = token_new(TT_MINUS, start, 1);
                }
                break;
            case '*':
                if (peek(lexer) == '=') {
                    consume(lexer);
                    token->next = token_new(TT_STAR_EQ, start, 2);
                } else {
                    token->next = token_new(TT_STAR, start, 1);
                }
                break;
            case '/':
                if (peek(lexer) == '=') {
                    consume(lexer);
                    token->next = token_new(TT_SLASH_EQ, start, 2);
                } else {
                    token->next = token_new(TT_SLASH, start, 1);
                }
                break;
            case '(':
                token->next = token_new(TT_PAREN_L, start, 1);
                break;
            case ')':
                token->next = token_new(TT_PAREN_R, start, 1);
                break;
            case '{':
                token->next = token_new(TT_BRACE_L, start, 1);
                break;
            case '}':
                token->next = token_new(TT_BRACE_R, start, 1);
                break;
            case '=':
                if (peek(lexer) != '=') {
                    token->next = token_new(TT_EQ, start, 1);
                } else {
                    consume(lexer);
                    token->next = token_new(TT_EQ_EQ, start, 2);
                }
                break;
            case '!':
                if (peek(lexer) != '=') {
                    token->next = token_new(TT_BANG, start, 1);
                } else {
                    consume(lexer);
                    token->next = token_new(TT_BANG_EQ, start, 2);
                }
                break;
            case '<':
                if (peek(lexer) != '=') {
                    token->next = token_new(TT_ANGLE_L, start, 1);
                } else {
                    consume(lexer);
                    token->next = token_new(TT_ANGLE_L_EQ, start, 2);
                }
                break;
            case '>':
                if (peek(lexer) != '=') {
                    token->next = token_new(TT_ANGLE_R, start, 1);
                } else {
                    consume(lexer);
                    token->next = token_new(TT_ANGLE_R_EQ, start, 2);
                }
                break;
            case ';':
                token->next = token_new(TT_SEMICOLON, start, 1);
                break;
            case ',':
                token->next = token_new(TT_COMMA, start, 1);
                break;
            default:
                if (isdigit(*start)) {
                    while (isdigit(peek(lexer))) end = consume(lexer);
                    token->next = token_new(TT_INT, start, end - start + 1);
                } else if (isalpha(*start)) {
                    while (isalnum(peek(lexer)) || peek(lexer) == '_') end = consume(lexer);
                    int len = end - start + 1;
                    TokenTag tag = lookup_ident(start, len); // TT_IDENT or TT_<keyword>
                    token->next = token_new(tag, start, end - start + 1);
                } else {
                    panic("tokenize error");
                }
                break;
        }
        token = token->next;
    }
    return head.next;
}
