#include "kcc.h"

Node *code[256];
Func *current_func;

Func *new_func() {
    Func *fn = calloc(1, sizeof(Func));
    fn->locals_num = 0;
    return fn;
}

Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *new_node_empty(NodeKind kind) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

Node *new_node_name(NodeKind kind, char *name) {
    Node *node = new_node_empty(kind);
    node->name = name;
    return node;
}

Node *new_node_num(int val) {
    Node *node = new_node_empty(ND_NUM);
    node->val = val;
    return node;
}

Node *new_node_lvar(Func *fn, Token *token) {
    Node *node = new_node_empty(ND_LVAR);
    LVar *var = calloc(1, sizeof(LVar));
    var->next = fn->locals;
    var->name = token->str;
    var->len = token->len;
    var->offset = fn->locals ? fn->locals->offset + 8 : 8;
    node->offset = var->offset;
    fn->locals = var;
    fn->locals_num++;
    return node;
}

Node *get_node_lvar(Func *fn, Token *token) {
    Node *node = new_node_empty(ND_LVAR);
    LVar *var = find_lvar(fn, token);
    node->offset = var->offset;
    return node;
}

LVar *find_lvar(Func *fn, Token *token) {
    for(LVar *var = fn->locals; var; var = var->next) {
        if(var->len == token->len && !memcmp(token->str, var->name, var->len))
            return var;
    }
    return NULL;
}

// TODO: var_decl周辺の定義

/*
 * program    = stmt*
 * var_decl   = IDENT
 * stmt       = expr ";" | "return" expr ";" | "int" "*"* var_decl ";" |
 *              "{" stmt* "}" |
 *              "if" "(" expr ")" stmt ("else" stmt)? |
 *              "while" "(" expr ")" stmt |
 *              "for" "(" expr? ";" expr? ";" expr?")" stmt
 * expr       = assign
 * assign     = equality ("=" assign)?
 * equality   = relational ("==" relational | "!=" relational)*
 * relational = add ("<" add | "<=" add | ">" add | ">=" add)*
 * add        = mul ("+" mul | "-" mul)
 * mul        = unary ("*" unary | "/" unary)*
 * unary      = ("+" | "-")? primary | "*" unary | "&" unary
 * primary    = NUM | IDENT ("(" fncall-args? ")")? | "(" expr ")"
 * fncall_args= expr ( "," expr  )*
 * funcdef    = IDENT "(" "int" var_decl ( ", int" var_decl)* ")" stmt
*/

void program() {
    int i = 0;
    while(!at_eof()) {
        current_func = new_func();
        code[i++] = funcdef();
        free(current_func);
    }
    code[i] = NULL; // 末尾
}

Node *var_decl() {
    return new_node_lvar(current_func, expect_ident());
}

Node *stmt() {
    Node *node;
    if(consume("return")) {
        node = new_node(ND_RETURN, expr(), NULL);
        expect(";");
    } else if(consume("if")) {
        node = stmt_if();
    } else if(consume("for")) {
        node = stmt_for();
    } else if(consume("while")) {
        node = stmt_while();
    } else if(consume("{")) {
        node = stmt_block();
    } else if(consume("int")) {
        Type *type = calloc(1, sizeof(Type));
        type->ty = INT;
        while(consume("*")) {
            Type *cur_type = calloc(1, sizeof(Type));
            cur_type->ty = PTR;
            cur_type->ptr_to = type;
            type = cur_type;
        }
        node = var_decl();
        node->type = type;
        expect(";");
    } else {
        node = expr();
        expect(";");
    }
    return node;
}

Node *stmt_if() {
    Node *node = new_node_empty(ND_IF);
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    if(consume("else")) node->els = stmt();
    return node;
}

Node *stmt_for() {
    Node *node = new_node_empty(ND_FOR);
    expect("(");
    if(!consume(";")) {
        node->init = expr();
        expect(";");
    }
    if(!consume(";")) {
        node->cond = expr();
        expect(";");
    }
    node->inc = expr();
    expect(")");
    node->lhs = stmt();
    return node;
}

Node *stmt_while() {
    Node *node = new_node_empty(ND_WHILE);
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    return node;
}

Node *stmt_block() {
    Node *head = new_node_empty(ND_BLOCK);
    head->body = stmt();
    Node *before = head;
    while(!consume("}")) {
        Node *node = new_node_empty(ND_BLOCK);
        before->next = node;
        node->body = stmt();
        before = node;
    }
    return head;
}

Node *expr() {
    return assign();
}

Node *assign() {
    Node *node = equality();
    if(consume("=")) {
        return new_node(ND_ASSIGN, node, assign());
    }
    return node;
}

Node *equality() {
    Node *node = relational();
    for(;;) {
        if(consume("=="))
            node = new_node(ND_EQ, node, relational());
        else if(consume("!="))
            node = new_node(ND_NE, node, relational());
        else
            return node;
    }
}

Node *relational() {
    Node *node = add();
    for(;;) {
        if(consume("<"))
            node = new_node(ND_LT, node, add());
        else if(consume("<="))
            node = new_node(ND_LE, node, add());
        else if(consume(">"))
            node = new_node(ND_LT, add(), node);
        else if(consume(">="))
            node = new_node(ND_LE, add(), node);
        else
            return node;
    }
}

Node *add() {
    Node *node = mul();
    for(;;) {
        if(consume("+"))
            node = new_node(ND_ADD, node, mul());
        else if(consume("-"))
            node = new_node(ND_SUB, node, mul());
        else
            return node;
    }
}

Node *mul() {
    Node *node = unary();
    for(;;) {
        if(consume("*"))
            node = new_node(ND_MUL, node, unary());
        else if(consume("/"))
            node = new_node(ND_DIV, node, unary());
        else
            return node;
    }
}

Node *unary() {
    if(consume("+"))
        return primary();
    if(consume("-"))
        return new_node(ND_SUB, new_node_num(0), primary());
    if(consume("&")) {
        Node *node = new_node_empty(ND_ADDR);
        node->lhs = unary();
        return node;
    }
    if(consume("*")) {
        Node *node = new_node_empty(ND_DEREF);
        node->lhs = unary();
        return node;
    }
    return primary();
}

Node *primary() {
    if(consume("(")) {
        Node *node = expr();
        expect(")");
        return node;
    }
    if(token_is(TK_IDENT)) {
        Token *t = expect_ident();
        if(consume("(")) {
            Node *node = new_node_name(ND_FNCALL, get_token_str(t));
            node->args = fncall_args();
            return node;
        }
        return get_node_lvar(current_func, t);
    }
    if(token_is(TK_NUM)) return new_node_num(expect_number());
    error_at(token->str, "parse error @primary.");
    exit(1);
}

Node *fncall_args() {
    if(consume(")")) return NULL;
    Node *head = new_node_empty(ND_ARGS);
    head->body = expr();
    Node *before = head;
    while(consume(",")) {
        Node *node = new_node_empty(ND_ARGS);
        before->next = node;
        node->body = expr();
        before = node;
    }
    expect(")");
    return head;
}

Node *funcdef() {
    expect("int");
    Token *t = expect_ident();
    Node *node = new_node_name(ND_FNDEF, get_token_str(t));
    expect("(");
    node->args = NULL;
    if(!consume(")")) {
        expect("int");
        Node *head = new_node_lvar(current_func, expect_ident());
        Node *cur = head;
        while(consume(",")) {
            expect("int");
            Node *n = new_node_lvar(current_func, expect_ident());
            cur->next = n;
            cur = n;
        }
        expect(")");
        node->args = head;
    }
    node->args_num = current_func->locals_num;
    node->body = stmt();
    node->locals_num = current_func->locals_num;
    return node;
}
