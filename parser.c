#include "kcc.h"


enum OpPrecedence {
    PREC_NONE = 0, // for syntax error check
    PREC_LOWEST,
    PREC_COMMA,
    PREC_ASSIGN,
    PREC_COND,
    PREC_LOGICAL,
    PREC_EQUALS,
    PREC_LESSGREATER,
    PREC_ADD,
    PREC_MUL,
    PREC_PREFIX,
    PREC_CALL,
    PREC_INDEX,
    PREC_MEMBER,
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
    [TT_PERCENT]    = PREC_MUL,
    [TT_QUESTION]   = PREC_COND,
    [TT_COMMA]      = PREC_COMMA,
    [TT_AND_AND]    = PREC_LOGICAL,
    [TT_PIPE_PIPE]  = PREC_LOGICAL,
    [TT_PERIOD]        = PREC_MEMBER,
    [TT_MINUS_ANGLE_R] = PREC_MEMBER,
    [TT_PAREN_L]    = PREC_INDEX,
    [TT_BRACKET_L]  = PREC_INDEX,
    // otherwise PREC_NONE (== 0)
};

static bool is_infix(TokenTag tag) {
    return precedences[tag] != PREC_NONE;
}

static bool is_right_assoc(TokenTag tag) {
    // left-associative : binding power (l), r > l, (r)
    // right-associative: binding power (l), r < l, (r)
    return tag == TT_EQ
        || tag == TT_PLUS_EQ
        || tag == TT_MINUS_EQ
        || tag == TT_STAR_EQ
        || tag == TT_SLASH_EQ;
}

static bool is_type_specifier(Token *token) {
    return token->tag == TT_KW_VOID
        || token->tag == TT_KW_INT
        || token->tag == TT_KW_CHAR
        || token->tag == TT_KW_STRUCT
        || token->tag == TT_KW_UNION
        || token->tag == TT_KW_ENUM;
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
    *locals = symbol;
    return symbol;
}

static Symbol *append_global_var(Parser *parser, Token *ident, Type *type, Node *init) {
    Symbol **globals = &parser->global_vars;
    Symbol *symbol = symbol_new(ST_GVAR, ident, type, *globals);
    symbol->init = init;
    *globals = symbol;
    return symbol;
}

static Symbol *append_func_type(Parser *parser, Token *ident, Type *type) {
    Symbol **fntypes = &parser->func_types;
    Symbol *symbol = symbol_new(ST_FUNC, ident, type, *fntypes);
    *fntypes = symbol;
    return symbol;
}

static Symbol *append_type(Parser *parser, SymbolTag tag, Token *ident, Type *type) {
    Symbol **deftypes = &parser->defined_types;
    Symbol *symbol = symbol_new(tag, ident, type, *deftypes);
    *deftypes = symbol;
    return symbol;
}

Symbol *find_symbol(SymbolTag tag, Symbol *symlist, Token *ident) {
    for (Symbol *sym = symlist; sym != NULL; sym = sym->next) {
        if (ident->len != sym->token->len) continue;
        if (sym->tag == tag && strncmp(sym->token->start, ident->start, sym->token->len) == 0) return sym;
    }
    return NULL;
}

Symbol *find_member(Symbol *symlist, Token *ident, int *offset) {
    for (Symbol *sym = symlist; sym != NULL; sym = sym->next) {
        if (sym->token == NULL) {
            // anonymous struct / union
            if (sym->type->tag != TYP_STRUCT && sym->type->tag != TYP_UNION)
                panic("invalid member");

            if (offset) *offset += sym->offset;
            Symbol *sub_sym = find_member(sym->type->tagged_typ.list, ident, offset);
            if (sub_sym) {
                return sub_sym;
            }
            if (offset) *offset -= sym->offset;
            continue;
        }
        if (ident->len != sym->token->len) continue;
        if (sym->tag == ST_MEMBER && strncmp(sym->token->start, ident->start, sym->token->len) == 0) {
            if (offset) *offset += sym->offset;
            return sym;
        }
    }
    return NULL;
}

Symbol *find_enum_val(Symbol *defined_types, Token *ident) {
    for (Symbol *sym = defined_types; sym != NULL; sym = sym->next) {
        if (sym->tag != ST_ENUM) continue;
        Symbol *mem = find_symbol(ST_MEMBER, sym->type->tagged_typ.list, ident);
        if (mem) return mem;
    }
    return NULL;
}

Symbol *reverse_symbols(Symbol *list) {
    Symbol *prev = NULL, *current = list;
    while (current) {
        Symbol *next = current->next;
        current->next = prev;
        prev = current;
        current = next;
    }
    return prev;
}

int align_n(int x, int n) {
    return ((x + n - 1) / n) * n;
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
        case TT_PLUS:       return expr;
        case TT_MINUS:      tag = NT_NEG; break;
        case TT_AMPERSAND:  tag = NT_ADDR; break;
        case TT_STAR:       tag = NT_DEREF; break;
        case TT_BANG:       tag = NT_BOOL_NOT; break;
        case TT_KW_SIZEOF:  tag = NT_SIZEOF; break;
        case TT_PLUS_PLUS:  tag = NT_PREINC; break;
        case TT_MINUS_MINUS:tag = NT_PREDEC; break;
        default: panic("unary_new: invalid token TokenTag=%d (%.*s)", token->tag, token->len, token->start);
    }
    Node *node = node_new(tag, token);
    node->unary_expr = expr;
    return node;
}

static Node *member_access_new(Token *token, Node *lhs, Node *member) {
    NodeTag tag;
    switch (token->tag) {
        case TT_PERIOD: tag = NT_DOT; break;
        case TT_MINUS_ANGLE_R: tag = NT_ARROW; break;
        default: panic("member_access_new: invalid token tag=%d", token->tag);
    }
    Node *node = node_new(tag, token);
    node->member_access.lhs = lhs;
    node->member_access.member = member;
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
        case TT_PERCENT:    tag = NT_MOD; break;
        case TT_EQ_EQ:      tag = NT_EQ; break;
        case TT_BANG_EQ:    tag = NT_NE; break;
        case TT_ANGLE_L:
        case TT_ANGLE_R:    tag = NT_LT; break;
        case TT_ANGLE_L_EQ:
        case TT_ANGLE_R_EQ: tag = NT_LE; break;
        case TT_BRACKET_L:  tag = NT_ADD; break; // for array access
        case TT_COMMA:      tag = NT_COMMA; break;
        case TT_AND_AND:    tag = NT_AND; break;
        case TT_PIPE_PIPE:  tag = NT_OR; break;
        case TT_PERIOD:
        case TT_MINUS_ANGLE_R:
            return member_access_new(token, lhs, rhs);
        default: panic("expr_new: invalid token tag=%d", token->tag);
    }
    Node *node = node_new(tag, token);
    node->expr.lhs = lhs;
    node->expr.rhs = rhs;
    return node;
}

static Node *postfix_new(Token *token, Node *expr) {
    NodeTag tag;
    switch (token->tag) {
        case TT_PLUS_PLUS: tag = NT_POSTINC; break;
        case TT_MINUS_MINUS: tag = NT_POSTDEC; break;
        default: panic("postfix_new: invalid token tag=%d", token->tag);
    }
    Node *node = node_new(tag, token);
    node->pre_expr = expr;
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

static Type *struct_decl(Parser *parser);
static Type *union_decl(Parser *parser);
static Type *decl_spec(Parser *parser);
static Type *pointer(Parser *parser, Type *type);
static Type *array(Parser *parser, Type *type);

static Node *try_typename(Parser *parser);
static Node *integer(Parser *parser);
static NodeList *args(Parser *parser);
static Node *unary(Parser *parser);
static Node *expr_prefix(Parser *parser);
static Node *expr_postfix(Parser *parser, Node *lhs);
static Node *expr_bp(Parser *parser, int min_bp);
static Node *expr_cond(Parser *parser, Token *token, Node *cond);
static Node *expr_fncall(Parser *parser, Token *token, Node *fn);
static Node *expr_array_index(Parser *parser, Token *token, Node *lhs);
static Node *expr(Parser *parser);
static Node *expr_disable_comma(Parser *parser);
static Node *block(Parser *parser);
static Node *if_stmt(Parser *parser);
static Node *while_stmt(Parser *parser);
static Node *do_while_stmt(Parser *parser);
static Node *for_stmt(Parser *parser);
static Node *switch_stmt(Parser *parser);
static Node *stmt(Parser *parser);
static Node *direct_declarator(Parser *parser, Type *type);
static Node *direct_abstract_declarator(Parser *parser, Type *type);
static Node *declarator(Parser *parser, Type *type);
static Node *abstract_declarator(Parser *parser, Type *type);
static Node *initializer(Parser *parser);
static Node *init_declarator(Parser *parser, Type *type);
static Node *local_decl(Parser *parser);
static Node *param_decl(Parser *parser);
static NodeList *params(Parser *parser);
static Node *func(Parser *parser, Type *return_type, Token *name);
static Node *toplevel(Parser *parser);

// parse type

static Symbol *struct_decl_list(Parser *parser) {
    Symbol *list = NULL;
    while (is_type_specifier(peek(parser))) {
        Type *type_spec = decl_spec(parser);
        Node *declr = NULL;
        if (peek(parser)->tag != TT_SEMICOLON) declr = declarator(parser, type_spec);
        list = symbol_new(ST_MEMBER,
                          declr ? declr->main_token : NULL,
                          declr ? declr->type : type_spec,
                          list);
        if (consume(parser)->tag != TT_SEMICOLON) panic("expected \';\'");
    }
    return list;
}

static Type *struct_decl(Parser *parser) {
    Node *ident = NULL;
    Type *struct_typ = struct_new(NULL, NULL, 0, 0); // placeholder
    if (peek(parser)->tag == TT_IDENT) {
        ident = ident_new(consume(parser));
        append_type(parser, ST_STRUCT, ident->main_token, struct_typ);
    }
    if (consume(parser)->tag != TT_BRACE_L) panic("expected \'{\'");

    Symbol *list = struct_decl_list(parser);
    list = reverse_symbols(list);

    int current_offset = 0;
    int align_max = 0;
    for (Symbol *elem = list; elem != NULL; elem = elem->next) {
        int align = alignof_type(elem->type);
        current_offset = align_n(current_offset, align);
        elem->offset = current_offset;
        current_offset += sizeof_type(elem->type);
        align_max = align > align_max ? align : align_max;
    }

    int total_size = align_n(current_offset, align_max);
    if (consume(parser)->tag != TT_BRACE_R) panic("expected \'}\'");
    struct_typ->tagged_typ.ident = ident;
    struct_typ->tagged_typ.list = list;
    struct_typ->tagged_typ.size = total_size;
    struct_typ->tagged_typ.align = align_max;
    if (ident) {
        Symbol *struct_sym = find_symbol(ST_STRUCT, parser->defined_types, ident->main_token);
        struct_sym->type = struct_typ;
    }
    return struct_typ;
}

static Type *union_decl(Parser *parser) {
    Node *ident = NULL;
    if (peek(parser)->tag == TT_IDENT) ident = ident_new(consume(parser));
    if (consume(parser)->tag != TT_BRACE_L) panic("expected \'{\'");

    Symbol *list = struct_decl_list(parser);
    list = reverse_symbols(list);

    int size_max = 0;
    int align_max = 0;
    for (Symbol *elem = list; elem != NULL; elem = elem->next) {
        elem->offset = 0;
        int align = alignof_type(elem->type);
        int size = sizeof_type(elem->type);
        align_max = align > align_max ? align : align_max;
        size_max = size > size_max ? size : size_max;
    }
    int total_size = align_n(size_max, align_max);
    if (consume(parser)->tag != TT_BRACE_R) panic("expected \'}\'");
    Type *union_typ = union_new(ident, list, total_size, align_max);
    if (ident) append_type(parser, ST_UNION, ident->main_token, union_typ);
    return union_typ;
}

static Type *enum_decl(Parser *parser) {
    Node *ident = NULL;
    Type *enum_typ = enum_new(ident, NULL);
    if (peek(parser)->tag == TT_IDENT) ident = ident_new(consume(parser));
    Symbol *list = NULL;
    if (consume(parser)->tag != TT_BRACE_L) panic("expected \'{\'");
    int index = 0;
    while (peek(parser)->tag == TT_IDENT) {
        Node *mnode = ident_new(consume(parser));
        list = symbol_new(ST_MEMBER, mnode->main_token, type_int, list);
        list->value = index++;
        if (peek(parser)->tag == TT_COMMA) consume(parser);
    }
    list = reverse_symbols(list);
    enum_typ->tagged_typ.list = list;
    if (consume(parser)->tag != TT_BRACE_R) panic("expected \'}\'");
    append_type(parser, ST_ENUM, ident ? ident->main_token : NULL, enum_typ);
    return enum_typ;
}

static Type *decl_spec(Parser *parser) {
    if (peek(parser)->tag == TT_KW_CONST) consume(parser); // ignore
    Token *token = consume(parser);
    if (token->tag == TT_KW_VOID) return type_void;
    else if (token->tag == TT_KW_CHAR) return type_char;
    else if (token->tag == TT_KW_INT) return type_int;
    else if (token->tag == TT_KW_STRUCT) {
        Token *next = peek(parser);
        if (next->tag == TT_IDENT) {
            Symbol *struct_sym = find_symbol(ST_STRUCT, parser->defined_types, next);
            if (struct_sym != NULL) {
                consume(parser); // struct tag
                return struct_sym->type;
            }
        }
        return struct_decl(parser);
    } else if (token->tag == TT_KW_UNION) {
        Token *next = peek(parser);
        if (next->tag == TT_IDENT) {
            Symbol *union_sym = find_symbol(ST_UNION, parser->defined_types, next);
            if (union_sym != NULL) {
                consume(parser); // union tag
                return union_sym->type;
            }
        }
        return union_decl(parser);
    } else if (token->tag == TT_KW_ENUM) {
        Token *next = peek(parser);
        if (next->tag == TT_IDENT) {
            Symbol *enum_sym = find_symbol(ST_ENUM, parser->defined_types, next);
            if (enum_sym != NULL) {
                consume(parser); // enum tag
                return enum_sym->type;
            }
        }
        return enum_decl(parser);
    }
    else panic("expected type");
    return NULL;
}

static Type *pointer(Parser *parser, Type *type) {
    while (peek(parser)->tag == TT_STAR) {
        consume(parser);
        type = pointer_to(type);
    }
    return type;
}

static Node *try_typename(Parser *parser) {
    Token *token = peek(parser);
    if (!is_type_specifier(token)) return NULL;
    Type *base = decl_spec(parser);
    Node *node = abstract_declarator(parser, base);
    return node;
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
        nodelist_append(args, expr_disable_comma(parser));
    } while (peek(parser)->tag == TT_COMMA && consume(parser));
    if (peek(parser)->tag != TT_PAREN_R) panic("expected \')\'");
    return args;
}

static Node *unary(Parser *parser) {
    Token *token = consume(parser);
    if (token->tag == TT_KW_SIZEOF) {
        if (peek(parser)->tag == TT_PAREN_L)  {
            consume(parser); // (
            Node *node = try_typename(parser);
            if (!node) node = expr(parser);
            if (consume(parser)->tag != TT_PAREN_R) panic("expected \')\'");
            return unary_new(token, node);
        }
    }
    return unary_new(token, expr_bp(parser, PREC_PREFIX));
}

static int int_from_charlit(Token *token) {
    const char *start = token->start + 1; // ignore '
    if (*start == '\\') {
        start++;
        switch (*start) {
            case '\\': return '\\';
            case '\'': return '\'';
            case '\"': return '\"';
            case 'n': return '\n';
            case 't': return '\t';
            case '0': if (*(start + 1) == '\'') return '\0';
        }
        panic("unknown escape sequence");
    }
    return *start;
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
        case TT_CHAR: {
            Token *token = consume(parser);
            node = int_new(token, int_from_charlit(token));
            break;
        }
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
        case TT_PLUS_PLUS:
        case TT_MINUS_MINUS:
            lhs = postfix_new(consume(parser), lhs);
            break;
        default: break;
    }
    return lhs;
}

static Node *expr_cond(Parser *parser, Token *token, Node *cond) {
    Node *then = expr(parser);
    if (consume(parser)->tag != TT_COLON) panic("expected \':\'");
    Node *els = expr(parser);
    Node *expr = node_new(NT_COND, token);
    expr->cond_expr.cond = cond;
    expr->cond_expr.then = then;
    expr->cond_expr.els = els;
    return expr;
}

static Node *expr_fncall(Parser *parser, Token *token, Node *fn) {
    NodeList *arg_nodes = args(parser);
    if (consume(parser)->tag != TT_PAREN_R) panic("expected \')\'"); // )
    return fncall_new(fn->main_token, fn, arg_nodes);
}

static Node *expr_array_index(Parser *parser, Token *token, Node *lhs) {
    Node *index = expr(parser);
    if (consume(parser)->tag != TT_BRACKET_R) panic("expected \']\'"); // ]
    Node *add = expr_new(token, lhs, index);
    lhs = node_new(NT_DEREF, token);
    lhs->unary_expr = add;
    return lhs;
}

static Node *expr_bp(Parser *parser, int min_bp) {
    Node *lhs = expr_prefix(parser);
    Token *token = peek(parser);
    while (is_infix(token->tag)) {
        int prec = precedences[token->tag];
        bool left_assoc = !is_right_assoc(token->tag);

        if (left_assoc && prec <= min_bp) break;
        else if (prec < min_bp) break;

        token = consume(parser);
        if (token->tag == TT_QUESTION) lhs = expr_cond(parser, token, lhs);
        else if (token->tag == TT_PAREN_L) lhs = expr_fncall(parser, token, lhs);
        else if (token->tag == TT_BRACKET_L) lhs = expr_array_index(parser, token, lhs);
        else {
            Node *rhs = expr_bp(parser, prec);
            lhs = expr_new(token, lhs, rhs);

            // if token is ">" or ">=", swap lhs, rhs
            if (token->tag == TT_ANGLE_R || token->tag == TT_ANGLE_R_EQ) {
                Node *tmp = lhs->expr.lhs;
                lhs->expr.lhs = lhs->expr.rhs;
                lhs->expr.rhs = tmp;
            }
        }
        token = peek(parser);
    }
    return lhs;
}

static Node *expr(Parser *parser) {
    return expr_bp(parser, PREC_LOWEST);
}

static Node *expr_disable_comma(Parser *parser) {
    return expr_bp(parser, PREC_ASSIGN);
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

static Node *do_while_stmt(Parser *parser) {
    Node *node = node_new(NT_DO_WHILE, consume(parser));
    node->whilestmt.body = stmt(parser);
    if (consume(parser)->tag != TT_KW_WHILE) panic("expected \'while\'");
    if (consume(parser)->tag != TT_PAREN_L) panic("expected \'(\'");
    node->whilestmt.cond = expr(parser);
    if (consume(parser)->tag != TT_PAREN_R) panic("expected \')\'");
    return node;
}

static Node *for_stmt(Parser *parser) {
    Node *node = node_new(NT_FOR, consume(parser));
    if (consume(parser)->tag != TT_PAREN_L) panic("expected \'(\'");

    // def
    if (peek(parser)->tag != TT_SEMICOLON) {
        node->forstmt.def = is_type_specifier(peek(parser)) ?
                            local_decl(parser) : expr(parser);
    }
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

static Node *case_block(Parser *parser) {
    Token *token = consume(parser);
    Node *node = node_new(NT_CASE, token);
    if (token->tag == TT_KW_CASE) node->caseblock.constant = expr(parser);
    else if (token->tag != TT_KW_DEFAULT) panic("expected case/default");
    if (consume(parser)->tag != TT_COLON) panic("expected \':\'");
    node->caseblock.stmts = nodelist_new(DEFAULT_NODELIST_CAP);
    token = peek(parser);
    while (token->tag != TT_KW_CASE && token->tag != TT_KW_DEFAULT && token->tag != TT_BRACE_R) {
        Node *stmtnode = stmt(parser);
        nodelist_append(node->caseblock.stmts, stmtnode);
        token = peek(parser);
    }
    return node;
}

static Node *switch_stmt(Parser *parser) {
    Node *node = node_new(NT_SWITCH, consume(parser));
    if (consume(parser)->tag != TT_PAREN_L) panic("expected \'(\'");
    node->switchstmt.control = expr(parser);
    if (consume(parser)->tag != TT_PAREN_R) panic("expected \')\'");
    if (consume(parser)->tag != TT_BRACE_L) panic("expected \'{\'");
    node->switchstmt.cases = nodelist_new(DEFAULT_NODELIST_CAP);
    Token *token = peek(parser);
    while (token->tag == TT_KW_CASE || token->tag == TT_KW_DEFAULT) {
        Node *casenode = case_block(parser);
        nodelist_append(node->switchstmt.cases, casenode);
        token = peek(parser);
    }
    if (consume(parser)->tag != TT_BRACE_R) panic("expected \'}\'");
    return node;
}

static Type *array(Parser *parser, Type *type) {
    int len = 0;
    int capacity = 4;
    int *stack = calloc(capacity, sizeof(int));
    while (peek(parser)->tag == TT_BRACKET_L) {
        consume(parser); // [
        int size = integer(parser)->integer;
        if (capacity <= len) {
            capacity *= 2;
            stack = realloc(stack, capacity * sizeof(int));
        }
        stack[len++] = size;
        if (consume(parser)->tag != TT_BRACKET_R) panic("expected \']\'");
    }
    for (int i = len - 1; 0 <= i; i--) {
        type = array_of(type, stack[i]);
    }
    return type;
}

static Node *direct_declarator(Parser *parser, Type *type) {
    Node *node;
    Token *token = peek(parser);
    Type *placeholder = calloc(1, sizeof(Type));
    if (token->tag == TT_IDENT) {
        node = ident_new(consume(parser));
        node->type = placeholder;
    } else if (token->tag == TT_PAREN_L) {
        consume(parser); // (
        *placeholder = *type;
        node = declarator(parser, placeholder);
        if (consume(parser)->tag != TT_PAREN_R) panic("expected \')\'");
    }
    *placeholder = *array(parser, type);
    return node;
}

static Node *direct_abstract_declarator(Parser *parser, Type *type) {
    Node *node;
    Token *token = peek(parser);
    Type *placeholder = calloc(1, sizeof(Type));
    if (token->tag == TT_PAREN_L) {
        consume(parser); // (
        *placeholder = *type;
        node = abstract_declarator(parser, placeholder);
        if (consume(parser)->tag != TT_PAREN_R) panic("expected \')\'");
    } else {
        node = node_new(NT_TYPENAME, NULL);
        node->type = placeholder;
    }
    *placeholder = *array(parser, type);
    return node;
}

// returns typed ident node
static Node *declarator(Parser *parser, Type *type) {
    type = pointer(parser, type);
    return direct_declarator(parser, type);
}

static Node *abstract_declarator(Parser *parser, Type *type) {
    type = pointer(parser, type);
    return direct_abstract_declarator(parser, type);
}

static Node *initializer(Parser *parser) {
    if (peek(parser)->tag == TT_BRACE_L) {
        Node *node = node_new(NT_INITS, consume(parser));
        node->initializers = nodelist_new(DEFAULT_NODELIST_CAP);
        NodeList *inits = node->initializers;
        do {
            if (peek(parser)->tag == TT_BRACE_R) break;
            Node *init = expr_disable_comma(parser);
            nodelist_append(inits, init);
        } while (peek(parser)->tag == TT_COMMA && consume(parser));
        if (consume(parser)->tag != TT_BRACE_R) panic("expected \'}\'");
        return node;
    }
    return expr_disable_comma(parser);
}

static Node *init_declarator(Parser *parser, Type *type) {
    Node *node = node_new(NT_DECLARATOR, peek(parser));
    node->declarator.name = declarator(parser, type);
    if (peek(parser)->tag == TT_EQ) {
        consume(parser); // =
        node->declarator.init = initializer(parser);
    }
    return node;
}

static Node *local_decl(Parser *parser) {
    Node *node = node_new(NT_LOCALDECL, peek(parser));
    node->declarators = nodelist_new(DEFAULT_NODELIST_CAP);
    Type *type_spec = decl_spec(parser);
    if (peek(parser)->tag == TT_SEMICOLON) return node;
    do {
        Node *dnode = init_declarator(parser, type_spec);
        Node *name = dnode->declarator.name;
        append_local_var(parser, name->main_token, name->type);
        nodelist_append(node->declarators, dnode);
    } while (peek(parser)->tag == TT_COMMA && consume(parser));
    return node;
}

static Node *global_decl(Parser *parser, Type *type_spec, Node *typed_ident) {
    Node *node = node_new(NT_GLOBALDECL, peek(parser));
    node->declarators = nodelist_new(DEFAULT_NODELIST_CAP);
    
    Node *first_dnode = node_new(NT_DECLARATOR, typed_ident->main_token);
    first_dnode->declarator.name = typed_ident;
    if (peek(parser)->tag == TT_EQ) {
        consume(parser); // =
        first_dnode->declarator.init = initializer(parser);
    }
    Node *name = first_dnode->declarator.name;
    Node *init = first_dnode->declarator.init;
    append_global_var(parser, name->main_token, name->type, init);
    nodelist_append(node->declarators, first_dnode);

    while (peek(parser)->tag == TT_COMMA && consume(parser)) {
        Node *dnode = init_declarator(parser, type_spec);
        name = dnode->declarator.name;
        init = dnode->declarator.init;
        append_global_var(parser, name->main_token, name->type, init);
        nodelist_append(node->declarators, dnode);
    }
    if (consume(parser)->tag != TT_SEMICOLON) panic("expected \';\'");
    return node;
}

static Node *param_decl(Parser *parser) {
    Node *node = node_new(NT_PARAMDECL, peek(parser));
    Type *type_spec = decl_spec(parser);
    Node *name = declarator(parser, type_spec);
    append_local_var(parser, name->main_token, name->type);
    node->ident = name;
    return node;
}

static Node *stmt(Parser *parser) {
    Node *node = NULL;
    switch (peek(parser)->tag) {
        case TT_BRACE_L:
            return block(parser);
        case TT_KW_IF:
            return if_stmt(parser);
        case TT_KW_WHILE:
            return while_stmt(parser);
        case TT_KW_DO:
            node = do_while_stmt(parser);
            break;
        case TT_KW_FOR:
            return for_stmt(parser);
        case TT_KW_SWITCH:
            return switch_stmt(parser);
        case TT_KW_VOID:
        case TT_KW_CHAR:
        case TT_KW_INT:
        case TT_KW_STRUCT:
        case TT_KW_UNION:
        case TT_KW_ENUM:
            node = local_decl(parser); 
            break;
        case TT_KW_RETURN:
            node = node_new(NT_RETURN, consume(parser));
            if (peek(parser)->tag != TT_SEMICOLON) node->unary_expr = expr(parser);
            break;
        case TT_KW_BREAK:
            node = node_new(NT_BREAK, consume(parser));
            break;
        case TT_KW_CONTINUE:
            node = node_new(NT_CONTINUE, consume(parser));
            break;
        case TT_SEMICOLON:
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
    else if (peek(parser)->tag == TT_KW_VOID) {
        consume(parser);
        return params;
    }
    do {
        Node *node = param_decl(parser);
        nodelist_append(params, node);
    } while (peek(parser)->tag == TT_COMMA && consume(parser));
    if (peek(parser)->tag != TT_PAREN_R) panic("expected \')\'");
    return params;
}

static Node *func(Parser *parser, Type *return_type, Token *name) {
    Node *node = node_new(NT_FUNC, name); 
    Symbol *fn_symbol = find_symbol(ST_FUNC, parser->func_types, name);
    if (!fn_symbol) append_func_type(parser, name, return_type);

    parser->current_func = node;
    node->func.locals = NULL;
    
    node->func.name = ident_new(name);
    if (consume(parser)->tag != TT_PAREN_L) panic("expected \'(\'");
    node->func.params = params(parser);
    if (consume(parser)->tag != TT_PAREN_R) panic("expected \')\'");
    if (peek(parser)->tag == TT_SEMICOLON && consume(parser)) return NULL;
    node->func.body = stmt(parser);
    return node;
}

static Node *toplevel(Parser *parser) {
    Type *type_spec = decl_spec(parser);
    if (peek(parser)->tag == TT_SEMICOLON && consume(parser)) return NULL;
    Node *node = declarator(parser, type_spec);
    Type *type = node->type;
    Token *ident_token = node->main_token;

    if (peek(parser)->tag == TT_PAREN_L) return func(parser, type, ident_token);
    return global_decl(parser, type_spec, node);
}

Program *parse(Parser *parser) {
    Program *prog = calloc(1, sizeof(Program));
    prog->funcs = nodelist_new(DEFAULT_NODELIST_CAP);
    while (peek(parser)->tag != TT_EOF) {
        Node *node = toplevel(parser);
        if (node && node->tag == NT_FUNC) nodelist_append(prog->funcs, node);
    }
    prog->func_types = parser->func_types;
    prog->global_vars = reverse_symbols(parser->global_vars);
    prog->string_tokens = parser->string_tokens;
    prog->defined_types = parser->defined_types;
    return prog;
}
