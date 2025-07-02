#include "kcc.h"

enum OpPrecedence {
    PREC_LOWEST = 0,
    PREC_EQUALS,
    PREC_LOGICAL,
    PREC_LESSGREATER,
    PREC_ADD,
    PREC_MUL,
    PREC_MOD,
    PREC_PREFIX,
    PREC_CALL,
    PREC_INDEX,
};

int precedences[] = {
    [TT_PLUS]  = PREC_ADD,
    [TT_MINUS] = PREC_ADD,
    [TT_STAR]  = PREC_MUL,
    [TT_SLASH] = PREC_MUL,
};

Parser *parser_new(Token *tokens) {
    Parser *parser = calloc(1, sizeof(Parser));
    parser->tokens = tokens;
    parser->current_token = tokens;
    return parser;
}

static Node *node_new(NodeTag tag, Token *main_token) {
    Node *node = calloc(1, sizeof(Node));
    node->tag = tag;
    node->main_token = main_token;
    return node;
}

static Node *int_new(Token *token, int val) {
    Node *node = node_new(NT_INT, token);
    node->integer = val;
    return node;
}

static Node *expr_new(Token *token, Node *lhs, Node *rhs) {
    NodeTag tag;
    switch (token->tag) {
        case TT_PLUS:  tag = NT_ADD; break;
        case TT_MINUS: tag = NT_SUB; break;
        case TT_STAR:  tag = NT_MUL; break;
        case TT_SLASH: tag = NT_DIV; break;
        default: panic("expr_new: invalid token tag=%d", token->tag);
    }
    Node *node = node_new(tag, token);
    node->expr.lhs = lhs;
    node->expr.rhs = rhs;
    return node;
}

static Token *peek(Parser *parser) {
    return parser->current_token;
}

static Token *consume(Parser *parser) {
    Token *token = parser->current_token;
    parser->current_token = token->next;
    return token;
}

static Node *integer(Parser *parser) {
    int val = 0;
    Token *token = consume(parser);
    if (token->tag != TT_INT) panic("expected an integer");
    const char *p = token->start;
    for (int i = 0; i < token->len; i++)
        val = val * 10 + (p[i] - '0');
    return int_new(token, val);
}

static Node *expr_bp(Parser *parser, int min_bp) {
    Node *lhs = integer(parser);
    while (peek(parser)->tag != TT_EOF) {
        int prec = precedences[peek(parser)->tag];
        if (min_bp >= prec) break;

        lhs = expr_new(consume(parser), lhs, NULL);
        lhs->expr.rhs = expr_bp(parser, prec);
    }
    return lhs;
}

static Node *expr(Parser *parser) {
    return expr_bp(parser, PREC_LOWEST);
}

Node *parse(Parser *parser) {
    return expr(parser);
}
