#include "kcc.h"

enum OpPrecedence {
    PREC_NONE = 0, // for syntax error check
    PREC_LOWEST,
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

int precedences[META_TT_NUM] = {
    [TT_EQ]         = PREC_ASSIGN,
    [TT_PLUS_EQ]    = PREC_ASSIGN,
    [TT_MINUS_EQ]   = PREC_ASSIGN,
    [TT_STAR_EQ]    = PREC_ASSIGN,
    [TT_SLASH_EQ]   = PREC_ASSIGN,
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
    // otherwise PREC_NONE (== 0)
};

typedef enum {
    ASSOC_LEFT,  // binding power (l), r > l, (r)
    ASSOC_RIGHT, // binding power (l), r < l, (r)
} OpAssoc;

OpAssoc assocs[] = {
    [TT_EQ]         = ASSOC_RIGHT,
    [TT_PLUS_EQ]    = ASSOC_RIGHT,
    [TT_MINUS_EQ]   = ASSOC_RIGHT,
    [TT_STAR_EQ]    = ASSOC_RIGHT,
    [TT_SLASH_EQ]   = ASSOC_RIGHT,
    [TT_EQ_EQ]      = ASSOC_LEFT,
    [TT_BANG_EQ]    = ASSOC_LEFT,
    [TT_ANGLE_L]    = ASSOC_LEFT,
    [TT_ANGLE_R]    = ASSOC_LEFT,
    [TT_ANGLE_L_EQ] = ASSOC_LEFT,
    [TT_ANGLE_R_EQ] = ASSOC_LEFT,
    [TT_PLUS]       = ASSOC_LEFT,
    [TT_MINUS]      = ASSOC_LEFT,
    [TT_STAR]       = ASSOC_LEFT,
    [TT_SLASH]      = ASSOC_LEFT,
    // [TT_PAREN_L]    = ASSOC_LEFT,
    // [TT_BRACKET_L]  = ASSOC_LEFT,
};

static bool is_infix(TokenTag tag) {
    return precedences[tag] != PREC_NONE;
}

// NodeList

NodeList *nodelist_new(int capacity) {
    NodeList *nlist = calloc(1, sizeof(NodeList));
    nlist->nodes = calloc(capacity, sizeof(Node*));
    nlist->len = 0;
    nlist->capacity = capacity;
    return nlist;
}

void nodelist_append(NodeList *nlist, Node *node) {
    if (nlist->capacity <= nlist->len) {
        nlist->capacity = nlist->capacity * 2 + 1;
        nlist->nodes = realloc(nlist->nodes, nlist->capacity * sizeof(Node*));
    }
    nlist->nodes[nlist->len++] = node;
}


// Var

Var *var_new(Token *ident, Type *type, int offset, Var *next) {
    Var *var = calloc(1, sizeof(Var));
    var->name = ident->start;
    var->len = ident->len;
    var->type = type;
    var->offset = offset;
    var->next = next;
    return var;
}

static Var *append_local_var(Parser *parser, Token *ident, Type *type, int size) {
    Var *locals = parser->current_func->func.locals;
    int current_offset = locals ? locals->offset : 0;
    Var *var = var_new(ident, type, current_offset + size, locals);
    parser->current_func->func.locals = var;
    return var;
}

Var *find_local_var(Var *env, Token *ident) {
    for (Var *var = env; var != NULL; var = var->next) {
        if (ident->len != var->len) continue;
        if (strncmp(var->name, ident->start, var->len) == 0) return var;
    }
    return NULL;
}

// Parser

Parser *parser_new(Token *tokens) {
    Parser *parser = calloc(1, sizeof(Parser));
    parser->tokens = tokens;
    parser->current_token = tokens;
    parser->current_func = NULL;
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

static Node *fncall_new(Token *token, Node *name, NodeList *args) {
    Node *node = node_new(NT_FNCALL, token);
    node->fncall.name = name;
    node->fncall.args = args;
    return node;
}

static Node *unary_new(Token *token, Node *expr) {
    NodeTag tag;
    switch (token->tag) {
        case TT_PLUS:   return expr;
        case TT_MINUS:  tag = NT_NEG; break;
        case TT_AMPERSAND:
                        tag = NT_ADDR; break;
        case TT_STAR:   tag = NT_DEREF; break;
        case TT_BANG:   tag = NT_BOOL_NOT; break;
        case TT_SIZEOF: tag = NT_SIZEOF; break;
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
        case TT_PLUS_EQ:    tag = NT_ASSIGN_ADD; break;
        case TT_MINUS_EQ:   tag = NT_ASSIGN_SUB; break;
        case TT_STAR_EQ:    tag = NT_ASSIGN_MUL; break;
        case TT_SLASH_EQ:   tag = NT_ASSIGN_DIV; break;
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
        case TT_BRACKET_L:  tag = NT_ADD; break; // for array access
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
static NodeList *args(Parser *parser);
static Node *unary(Parser *parser);
static Node *expr_prefix(Parser *parser);
static Node *expr_postfix(Parser *parser, Node *lhs);
static Node *expr_bp(Parser *parser, int min_bp);
static Node *expr(Parser *parser);
static Node *block(Parser *parser);
static Node *if_stmt(Parser *parser);
static Node *while_stmt(Parser *parser);
static Node *for_stmt(Parser *parser);
static Node *stmt(Parser *parser);
static Type *decl_spec(Parser *parser);
static Type *pointer(Parser *parser, Type *type);
static Node *decl(Parser *parser);
static Node *param_decl(Parser *parser);
static NodeList *params(Parser *parser);
static Node *func(Parser *parser);

static Node *integer(Parser *parser) {
    int val = 0;
    Token *token = consume(parser);
    if (token->tag != TT_INT) panic("expected an integer");
    const char *p = token->start;
    for (int i = 0; i < token->len; i++)
        val = val * 10 + (p[i] - '0');
    return int_new(token, val);
}

static NodeList *args(Parser *parser) {
    NodeList *args = nodelist_new(DEFAULT_NODELIST_CAP);
    if (peek(parser)->tag == TT_PAREN_R) return args;
    do {
        nodelist_append(args, expr(parser));
    } while (peek(parser)->tag == TT_COMMA && consume(parser));
    if (peek(parser)->tag != TT_PAREN_R) panic("expected \')\'");
    return args;
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
            node = ident_new(consume(parser));
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
    return expr_postfix(parser, node);
}

static Node *expr_postfix(Parser *parser, Node *lhs) {
    switch (peek(parser)->tag) {
        case TT_PAREN_L: {
            NodeList *arg_nodes;
            consume(parser); // (
            arg_nodes = args(parser);
            if (consume(parser)->tag != TT_PAREN_R) panic("expected \')\'"); // )
            lhs = fncall_new(lhs->main_token, lhs, arg_nodes);
            break;
        }
        case TT_BRACKET_L: {
            Token *lbracket = consume(parser); // [
            Node *index = expr(parser);
            if (consume(parser)->tag != TT_BRACKET_R) panic("expected \']\'"); // ]
            Node *add = expr_new(lbracket, lhs, index);
            lhs = node_new(NT_DEREF, lbracket);
            lhs->unary_expr = add;
            break;
        }
        case TT_PLUS_PLUS: panic("not implemented"); break;
        case TT_MINUS_MINUS: panic("not implemented"); break;
        default: break;
    }
    return lhs;
}

static Node *expr_bp(Parser *parser, int min_bp) {
    Node *lhs = expr_prefix(parser);
    while (is_infix(peek(parser)->tag)) {
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

static Node *block(Parser *parser) {
    Node *node = node_new(NT_BLOCK, consume(parser));
    node->block = nodelist_new(DEFAULT_NODELIST_CAP);
    while (peek(parser)->tag != TT_BRACE_R) {
        nodelist_append(node->block, stmt(parser));
    }
    if (consume(parser)->tag != TT_BRACE_R) panic("expected \'}\'");
    return node;
}

static Node *if_stmt(Parser *parser) {
    Node *node = node_new(NT_IF, consume(parser));
    if (consume(parser)->tag != TT_PAREN_L) panic("expected \'(\'");
    node->ifstmt.cond = expr(parser);
    if (consume(parser)->tag != TT_PAREN_R) panic("expected \')\'");
    node->ifstmt.then = stmt(parser);
    if (peek(parser)->tag == TT_ELSE) {
        consume(parser);
        node->ifstmt.els = stmt(parser);
    } else {
        node->ifstmt.els = NULL;
    }
    return node;
}

static Node *while_stmt(Parser *parser) {
    Node *node = node_new(NT_WHILE, consume(parser));
    if (consume(parser)->tag != TT_PAREN_L) panic("expected \'(\'");
    node->whilestmt.cond = expr(parser);
    if (consume(parser)->tag != TT_PAREN_R) panic("expected \')\'");
    node->whilestmt.body = stmt(parser);
    return node;
}

static Node *for_stmt(Parser *parser) {
    Node *node = node_new(NT_FOR, consume(parser));
    if (consume(parser)->tag != TT_PAREN_L) panic("expected \'(\'");

    // def
    if (peek(parser)->tag != TT_SEMICOLON) node->forstmt.def = expr(parser);
    consume(parser);
    // cond
    if (peek(parser)->tag != TT_SEMICOLON) node->forstmt.cond = expr(parser);
    consume(parser);
    // next
    if (peek(parser)->tag != TT_PAREN_R) node->forstmt.next = expr(parser);

    if (consume(parser)->tag != TT_PAREN_R) panic("expected \')\'");
    node->forstmt.body = stmt(parser);
    return node;
}

static Type *decl_spec(Parser *parser) {
    if (consume(parser)->tag != TT_KW_INT) panic("expected \"int\"");
    return type_int;
}

static Type *pointer(Parser *parser, Type *type) {
    while (peek(parser)->tag == TT_STAR) {
        consume(parser);
        type = pointer_to(type);
    }
    return type;
}

static Node *decl(Parser *parser) {
    Node *node = param_decl(parser);
    if (consume(parser)->tag != TT_SEMICOLON) panic("expected \';\'");
    return node;
}

static Node *param_decl(Parser *parser) {
    Type *base = decl_spec(parser);
    Type *type = pointer(parser, base);
    Token *ident_token = consume(parser);
    if (peek(parser)->tag == TT_BRACKET_L) {
        consume(parser); // [
        int size = integer(parser)->integer;
        type = array_of(type, size);
        if (consume(parser)->tag != TT_BRACKET_R) panic("expected \']\'");
    }
    Node *node = node_new(NT_VARDECL, ident_token);
    node->unary_expr = ident_new(ident_token);
    append_local_var(parser, ident_token, type, sizeof_type(type));
    return node;
}

static Node *stmt(Parser *parser) {
    Node *node;
    switch (peek(parser)->tag) {
        case TT_BRACE_L:
            return block(parser);
        case TT_IF:
            return if_stmt(parser);
        case TT_WHILE:
            return while_stmt(parser);
        case TT_FOR:
            return for_stmt(parser);
        case TT_KW_INT:
            return decl(parser); 
        case TT_RETURN:
            node = node_new(NT_RETURN, consume(parser));
            node->unary_expr = expr(parser);
            break;
        default:
            node = expr(parser);
            break;
    }
    if (consume(parser)->tag != TT_SEMICOLON) panic("expected \';\'");
    return node;
}

static NodeList *params(Parser *parser) {
    NodeList *params = nodelist_new(DEFAULT_NODELIST_CAP);
    if (peek(parser)->tag == TT_PAREN_R) return params;
    do {
        Node *node = param_decl(parser);
        nodelist_append(params, node);
        append_local_var(parser, node->main_token, type_int, sizeof_type(type_int)); // TODO:fix
    } while (peek(parser)->tag == TT_COMMA && consume(parser));
    if (peek(parser)->tag != TT_PAREN_R) panic("expected \')\'");
    return params;
}

static Node *func(Parser *parser) {
    Node *node = node_new(NT_FUNC, peek(parser));

    parser->current_func = node;
    node->func.locals = NULL;
    
    if (consume(parser)->tag != TT_KW_INT) panic("expected return type");

    node->func.name = ident_new(consume(parser));
    if (consume(parser)->tag != TT_PAREN_L) panic("expected \'(\'");
    node->func.params = params(parser);
    if (consume(parser)->tag != TT_PAREN_R) panic("expected \')\'");
    node->func.body = stmt(parser);
    return node;
}

NodeList *parse(Parser *parser) {
    NodeList *program = nodelist_new(DEFAULT_NODELIST_CAP);
    while (peek(parser)->tag != TT_EOF) {
        Node *node = func(parser);
        node = typed(node, node->func.locals); // add type info to nodes
        nodelist_append(program, node);
    }
    return program;
}
