#include "kcc.h"

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

Token *tokenize(Lexer *lexer) {
    Token head, *token = &head;
    while (token->tag != TT_EOF) {
        skip_space(lexer);
        const char *start = &lexer->input[lexer->pos], *end;
        char c = peek(lexer);

        if (c == '\0') {
            token->next = token_new(TT_EOF, consume(lexer), 1);
            break;
        } else if (c == '+') {
            token->next = token_new(TT_PLUS, consume(lexer), 1);
        } else if (c == '-') {
            token->next = token_new(TT_MINUS, consume(lexer), 1);
        } else if (c == '*') {
            token->next = token_new(TT_STAR, consume(lexer), 1);
        } else if (c == '/') {
            token->next = token_new(TT_SLASH, consume(lexer), 1);
        } else if (isdigit(c)) {
            while (isdigit(peek(lexer))) end = consume(lexer);
            token->next = token_new(TT_INT, start, end - start + 1);
        } else if (isalpha(c)) {
            while (isalnum(peek(lexer) || peek(lexer) == '_')) end = consume(lexer);
            token->next = token_new(TT_IDENT, start, end - start + 1);
        } else {
            fprintf(stderr, "tokenize error\n");
            exit(1);
        }
        token = token->next;
    }
    return head.next;
}
