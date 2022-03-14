#include "kcc.h"

Node *code[256];
LVar *locals;

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

Node *new_node_num(int val) {
    Node *node = new_node_empty(ND_NUM);
    node->val = val;
    return node;
}

Node *new_node_lvar(Token *token) {
    Node *node = new_node_empty(ND_LVAR);
    LVar *var = find_lvar(token);
    if(var) {
        node->offset = var->offset;
    } else {
        var = calloc(1, sizeof(LVar));
        var->next = locals;
        var->name = token->str;
        var->len = token->len;
        var->offset = locals ? locals->offset + 8 : 0;
        node->offset = var->offset;
        locals = var;
    }
    return node;
}

LVar *find_lvar(Token *token) {
    for(LVar *var = locals; var; var = var->next) {
        if(var->len == token->len && !memcmp(token->str, var->name, var->len))
            return var;
    }
    return NULL;
}

/*
 * program    = stmt*
 * stmt       = expr ";" | "return" expr ";" |
 *              "if" "(" expr ")" stmt ("else" stmt)?
 *              "while" "(" expr ")" stmt
 *              "for" "(" expr? ";" expr? ";" expr?")" stmt
 * expr       = assign
 * assign     = equality ("=" assign)?
 * equality   = relational ("==" relational | "!=" relational)*
 * relational = add ("<" add | "<=" add | ">" add | ">=" add)*
 * add        = mul ("+" mul | "-" mul)
 * mul        = unary ("*" unary | "/" unary)*
 * unary      = ("+" | "-")? primary
 * primary    = num | "(" expr ")"
*/

void program() {
    int i = 0;
    while(!at_eof()) code[i++] = stmt();
    code[i] = NULL; // 末尾
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
    if(!consume(";")) {
        node->inc = expr();
        expect(";");
    }
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
    return primary();
}

Node *primary() {
    if(consume("(")) {
        Node *node = expr();
        expect(")");
        return node;
    }
    if(token_is(TK_IDENT)) return new_node_lvar(expect_ident());
    if(token_is(TK_NUM)) return new_node_num(expect_number());
    error_at(token->str, "parse error @primary.");
    exit(1);
}

