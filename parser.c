#include "kcc.h"

enum OpPrecedence {
    PREC_LOWEST = 0,
    PREC_ASSIGN,
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
    [TT_EQ]         = PREC_ASSIGN,
    [TT_EQ_EQ]      = PREC_EQUALS,
    [TT_BANG_EQ]    = PREC_EQUALS,
    [TT_ANGLE_L]    = PREC_LESSGREATER,
    [TT_ANGLE_R]    = PREC_LESSGREATER,
    [TT_ANGLE_L_EQ] = PREC_LESSGREATER,
    [TT_ANGLE_R_EQ] = PREC_LESSGREATER,
    [TT_PLUS]       = PREC_ADD,
    [TT_MINUS]      = PREC_ADD,
    [TT_STAR]       = PREC_MUL,
    [TT_SLASH]      = PREC_MUL,
    [TT_PAREN_L]    = PREC_CALL,
};

typedef enum {
    ASSOC_LEFT,  // binding power (l), r > l, (r)
    ASSOC_RIGHT, // binding power (l), r < l, (r)
} OpAssoc;

OpAssoc assocs[] = {
    [TT_EQ]         = ASSOC_RIGHT,
    [TT_PLUS]       = ASSOC_LEFT,
    [TT_MINUS]      = ASSOC_LEFT,
    [TT_STAR]       = ASSOC_LEFT,
    [TT_SLASH]      = ASSOC_LEFT,
    [TT_PAREN_L]    = ASSOC_LEFT,
    [TT_EQ_EQ]      = ASSOC_LEFT,
    [TT_BANG_EQ]    = ASSOC_LEFT,
    [TT_ANGLE_L]    = ASSOC_LEFT,
    [TT_ANGLE_R]    = ASSOC_LEFT,
    [TT_ANGLE_L_EQ] = ASSOC_LEFT,
    [TT_ANGLE_R_EQ] = ASSOC_LEFT,
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

static Node *ident_new(Token *token) {
    return node_new(NT_IDENT, token);
}

static Node *unary_new(Token *token, Node *expr) {
    NodeTag tag;
    switch (token->tag) {
        case TT_PLUS:  return expr;
        case TT_MINUS: tag = NT_NEG; break;
        case TT_BANG:  tag = NT_BOOL_NOT; break;
        default: panic("unary_new: invalid token TokenTag=%d", token->tag);
    }
    Node *node = node_new(tag, token);
    node->unary_expr = expr;
    return node;
}

static Node *expr_new(Token *token, Node *lhs, Node *rhs) {
    NodeTag tag;
    switch (token->tag) {
        case TT_EQ:         tag = NT_ASSIGN; break;
        case TT_PLUS:       tag = NT_ADD; break;
        case TT_MINUS:      tag = NT_SUB; break;
        case TT_STAR:       tag = NT_MUL; break;
        case TT_SLASH:      tag = NT_DIV; break;
        case TT_EQ_EQ:      tag = NT_EQ; break;
        case TT_BANG_EQ:    tag = NT_NE; break;
        case TT_ANGLE_L:
        case TT_ANGLE_R:    tag = NT_LT; break;
        case TT_ANGLE_L_EQ:
        case TT_ANGLE_R_EQ: tag = NT_LE; break;
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

static Node *integer(Parser *parser);
static Node *identifier(Parser *parser);
static Node *unary(Parser *parser);
static Node *expr_prefix(Parser *parser);
static Node *expr_bp(Parser *parser, int min_bp);
static Node *expr(Parser *parser);

static Node *integer(Parser *parser) {
    int val = 0;
    Token *token = consume(parser);
    if (token->tag != TT_INT) panic("expected an integer");
    const char *p = token->start;
    for (int i = 0; i < token->len; i++)
        val = val * 10 + (p[i] - '0');
    return int_new(token, val);
}

static Node *identifier(Parser *parser) {
    Token *token = consume(parser);
    if (token->tag != TT_IDENT) panic("expected an identifier");
    return ident_new(token);
}

static Node *unary(Parser *parser) {
    Token *token = consume(parser);
    return unary_new(token, expr_bp(parser, PREC_PREFIX));
}

static Node *expr_prefix(Parser *parser) {
    Node *node;
    switch (peek(parser)->tag) {
        case TT_INT:
            node = integer(parser);
            break;
        case TT_IDENT:
            node = identifier(parser);
            break;
        case TT_PAREN_L:
            consume(parser);
            node = expr(parser);
            if (consume(parser)->tag != TT_PAREN_R) panic("expected \')\'");
            break;
        default:
            node = unary(parser);
            break;
    }
    return node;
}

static Node *expr_bp(Parser *parser, int min_bp) {
    Node *lhs = expr_prefix(parser);
    while (peek(parser)->tag != TT_EOF) {
        int prec = precedences[peek(parser)->tag];
        int assoc = assocs[peek(parser)->tag];
        if (assoc == ASSOC_LEFT && prec <= min_bp) break;
        else if (assoc == ASSOC_RIGHT && prec < min_bp) break;

        Token *token = consume(parser);
        Node *rhs = expr_bp(parser, prec);
        lhs = expr_new(token, lhs, rhs);

        // if token is ">" or ">=", swap lhs, rhs
        if (token->tag == TT_ANGLE_R || token->tag == TT_ANGLE_R_EQ) {
            Node *tmp = lhs->expr.lhs;
            lhs->expr.lhs = lhs->expr.rhs;
            lhs->expr.rhs = tmp;
        }
    }
    return lhs;
}

static Node *expr(Parser *parser) {
    return expr_bp(parser, PREC_LOWEST);
}

Node *parse(Parser *parser) {
    return expr(parser);
}
