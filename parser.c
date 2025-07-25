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

static bool is_typename(Token *token) {
    return token->tag == TT_KW_INT || token->tag == TT_KW_CHAR;
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

// TokenList

TokenList *tokenlist_new(int capacity) {
    TokenList *tlist = calloc(1, sizeof(TokenList));
    tlist->tokens = calloc(capacity, sizeof(Token*));
    tlist->len = 0;
    tlist->capacity = capacity;
    return tlist;
}

void tokenlist_append(TokenList *tlist, Token *token) {
    if (tlist->capacity <= tlist->len) {
        tlist->capacity = tlist->capacity * 2 + 1;
        tlist->tokens = realloc(tlist->tokens, tlist->capacity * sizeof(Token*));
    }
    tlist->tokens[tlist->len++] = token;
}

// Symbol

static Symbol *symbol_new(SymbolTag tag, Token *ident, Type *type, Symbol *next) {
    Symbol *symbol = calloc(1, sizeof(Symbol));
    symbol->tag = tag;
    symbol->token = ident;
    symbol->type = type;
    symbol->next = next;
    return symbol;
}

static Symbol *append_local_var(Parser *parser, Token *ident, Type *type) {
    Symbol **locals = &parser->current_func->func.locals;
    int current_offset = *locals ? (*locals)->offset : 0;
    int size = sizeof_type(type);
    Symbol *symbol = symbol_new(ST_LVAR, ident, type, *locals);
    symbol->offset = current_offset + size;
    (*locals) = symbol;
    return symbol;
}

static Symbol *append_global_var(Parser *parser, Token *ident, Type *type) {
    Symbol **globals = &parser->global_vars;
    Symbol *symbol = symbol_new(ST_GVAR, ident, type, *globals);
    (*globals) = symbol;
    return symbol;
}

static Symbol *append_func_type(Parser *parser, Token *ident, Type *type) {
    Symbol **fntypes = &parser->func_types;
    Symbol *symbol = symbol_new(ST_FUNC, ident, type, *fntypes);
    (*fntypes) = symbol;
    return symbol;
}

Symbol *find_symbol(SymbolTag tag, Symbol *symlist, Token *ident) {
    for (Symbol *sym = symlist; sym != NULL; sym = sym->next) {
        if (ident->len != sym->token->len) continue;
        if (sym->tag == tag && strncmp(sym->token->start, ident->start, sym->token->len) == 0) return sym;
    }
    return NULL;
}

// Parser

Parser *parser_new(Token *tokens) {
    Parser *parser = calloc(1, sizeof(Parser));
    parser->tokens = tokens;
    parser->current_token = tokens;
    parser->current_func = NULL;
    parser->global_vars = NULL;
    parser->func_types = NULL;
    parser->string_tokens = tokenlist_new(DEFAULT_TOKENLIST_CAP);
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

static Node *string_new(TokenList *tlist, Token *token) {
    Node *node = node_new(NT_STRING, token);
    node->index = tlist->len;
    tokenlist_append(tlist, token);
    return node;
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
        case TT_KW_SIZEOF:
                        tag = NT_SIZEOF; break;
        default: panic("unary_new: invalid token TokenTag=%d", token->tag);
    }
    Node *node = node_new(tag, token);
    node->unary_expr = expr;
    return node;
}

static Node *typenode_new(Token *token, Type *type) {
    Node *node = node_new(NT_TYPE, token);
    node->type = type;
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

static Type *decl_spec(Parser *parser);
static Type *pointer(Parser *parser, Type *type);
static Type *try_parse_typename(Parser *parser);

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
static Node *decl(Parser *parser);
static Node *param_decl(Parser *parser);
static NodeList *params(Parser *parser);
static Node *func(Parser *parser, Type *return_type, Token *name);
static Node *toplevel(Parser *parser);

// parse type

static Type *decl_spec(Parser *parser) {
    Token *token = consume(parser);
    if (token->tag == TT_KW_CHAR) return type_char;
    else if (token->tag == TT_KW_INT) return type_int;
    else panic("expected type");
    return type_int;
}

static Type *pointer(Parser *parser, Type *type) {
    while (peek(parser)->tag == TT_STAR) {
        consume(parser);
        type = pointer_to(type);
    }
    return type;
}

static Type *try_parse_typename(Parser *parser) {
    Token *token = peek(parser);
    if (!is_typename(token)) return NULL;
    Type *base = decl_spec(parser);
    Type *type = pointer(parser, base);
    return type;
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
    if (token->tag == TT_KW_SIZEOF) {
        if (peek(parser)->tag == TT_PAREN_L)  {
            consume(parser); // (
            Token *type_token = peek(parser);
            Type *type = try_parse_typename(parser);
            Node *expr = type ? typenode_new(type_token, type) : expr_bp(parser, PREC_LOWEST);
            if (consume(parser)->tag != TT_PAREN_R) panic("expected \')\'");
            return unary_new(token, expr);
        }
    }
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
        case TT_STRING:
            node = string_new(parser->string_tokens, consume(parser));
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
    if (peek(parser)->tag == TT_KW_ELSE) {
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

// identifier or assign expr
static Node *declarator(Parser *parser, Type *type) {
    type = pointer(parser, type);
    Token *ident_token = consume(parser);
    Node *node = node_new(NT_IDENT, ident_token);
    if (peek(parser)->tag == TT_BRACKET_L) {
        consume(parser); // [
        int size = integer(parser)->integer;
        type = array_of(type, size);
        if (consume(parser)->tag != TT_BRACKET_R) panic("expected \']\'");
    }
    if (peek(parser)->tag == TT_EQ) {
        Token *token_eq = consume(parser);
        node = expr_new(token_eq, node, expr(parser));
    }
    append_local_var(parser, ident_token, type);
    return node;
}

static Node *decl(Parser *parser) {
    Node *node = node_new(NT_LVARDECL, peek(parser));
    node->declarators = nodelist_new(DEFAULT_NODELIST_CAP);
    Type *type_spec = decl_spec(parser);
    if (!type_spec) panic("expected variable declaration");
    do {
        Node *dnode = declarator(parser, type_spec);
        nodelist_append(node->declarators, dnode);
    } while (peek(parser)->tag == TT_COMMA && consume(parser));
    if (consume(parser)->tag != TT_SEMICOLON) panic("expected \';\'");
    return node;
}

static Node *param_decl(Parser *parser) {
    Type *type = try_parse_typename(parser);
    if (!type) panic("expected variable declaration");
    Token *ident_token = consume(parser);
    if (peek(parser)->tag == TT_BRACKET_L) {
        consume(parser); // [
        int size = integer(parser)->integer;
        type = array_of(type, size);
        if (consume(parser)->tag != TT_BRACKET_R) panic("expected \']\'");
    }
    Node *node = node_new(NT_PARAMDECL, ident_token);
    node->unary_expr = ident_new(ident_token);
    append_local_var(parser, ident_token, type);
    return node;
}

static Node *stmt(Parser *parser) {
    Node *node;
    switch (peek(parser)->tag) {
        case TT_BRACE_L:
            return block(parser);
        case TT_KW_IF:
            return if_stmt(parser);
        case TT_KW_WHILE:
            return while_stmt(parser);
        case TT_KW_FOR:
            return for_stmt(parser);
        case TT_KW_CHAR:
        case TT_KW_INT:
            return decl(parser); 
        case TT_KW_RETURN:
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
    } while (peek(parser)->tag == TT_COMMA && consume(parser));
    if (peek(parser)->tag != TT_PAREN_R) panic("expected \')\'");
    return params;
}

static Node *func(Parser *parser, Type *return_type, Token *name) {
    Node *node = node_new(NT_FUNC, name); 
    append_func_type(parser, name, return_type);

    parser->current_func = node;
    node->func.locals = NULL;
    
    node->func.name = ident_new(name);
    if (consume(parser)->tag != TT_PAREN_L) panic("expected \'(\'");
    node->func.params = params(parser);
    if (consume(parser)->tag != TT_PAREN_R) panic("expected \')\'");
    node->func.body = stmt(parser);
    return node;
}

static Node *toplevel(Parser *parser) {
    Node *node;
    Type *type = try_parse_typename(parser);
    if (!type) panic("expected variable declaration");
    Token *ident_token = consume(parser);

    if (peek(parser)->tag == TT_PAREN_L) node = func(parser, type, ident_token);
    else {
        if (peek(parser)->tag == TT_BRACKET_L) {
            consume(parser); // [
            int size = integer(parser)->integer;
            type = array_of(type, size);
            if (consume(parser)->tag != TT_BRACKET_R) panic("expected \']\'");
        }
        append_global_var(parser, ident_token, type);
        node = node_new(NT_GLOBALDECL, ident_token);
        if (consume(parser)->tag != TT_SEMICOLON) panic("expected \';\'");
    }
    return node;
}

Program *parse(Parser *parser) {
    Program *prog = calloc(1, sizeof(Program));
    prog->funcs = nodelist_new(DEFAULT_NODELIST_CAP);
    while (peek(parser)->tag != TT_EOF) {
        Node *node = toplevel(parser);
        if (node->tag == NT_FUNC) nodelist_append(prog->funcs, node);
    }
    prog->func_types = parser->func_types;
    prog->global_vars = parser->global_vars;
    prog->string_tokens = parser->string_tokens;
    return prog;
}
