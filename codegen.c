#include "kcc.h"


static char *argreg[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

void print_token(Token *token) {
    const char *start = token->start;
    int len = token->len;
    printf("%.*s", len, start);
}

static void gen_local_var(Node *node, Var *env);
static void gen_fncall(Node *node, Var *env);
static void gen_expr(Node *node, Var *env);
static void gen_stmt(Node *node, Var *env);

static int align_16(int n) {
    // return ((n + 15) / 16) * 16;
    return (n + 15) & ~0xF;
}

static void gen_local_var(Node *node, Var *env) {
    if (node->tag != NT_IDENT) panic("expected an identifier");
    Var *var = find_local_var(env, node->main_token);
    if (var == NULL) panic("undefined variable");
    int offset = var->offset;
    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", offset);
    printf("  push rax\n");
}

static void gen_fncall(Node *node, Var *env) {
    Node **nodes = node->fncall.args->nodes;
    int narg = node->fncall.args->len;
    if (sizeof(argreg) / sizeof(char*) < narg) panic("too many args");
    for (int i = 0; i < narg; i++) gen_expr(nodes[i], env);
    for (int i = narg - 1; 0 <= i; i--) {
        printf("  pop rax\n");
        printf("  mov %s, rax\n", argreg[i]);
    }
    printf("  call ");
    print_token(node->main_token);
    printf("\n");
    printf("  push rax\n");
}

static void gen_expr(Node *node, Var *env) {
    if (node->tag == NT_INT) {
        printf("  push %d\n", node->integer);
        return;
    } else if (node->tag == NT_NEG) {
        gen_expr(node->unary_expr, env);
        printf("  pop rax\n");
        printf("  neg rax\n");
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_BOOL_NOT) {
        gen_expr(node->unary_expr, env);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  sete al\n");
        printf("  movzx rax, al\n");
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_IDENT) {
        gen_local_var(node, env);
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_ASSIGN) {
        gen_local_var(node->expr.lhs, env);
        gen_expr(node->expr.rhs, env);
        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  mov [rax], rdi\n");
        printf("  push rdi\n");
        return;
    } else if (node->tag == NT_FNCALL) {
        gen_fncall(node, env);
        return;
    }

    gen_expr(node->expr.lhs, env);
    gen_expr(node->expr.rhs, env);

    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (node->tag) {
        case NT_ADD:
            printf("  add rax, rdi\n");
            break;
        case NT_SUB:
            printf("  sub rax, rdi\n");
            break;
        case NT_MUL:
            printf("  imul rax, rdi\n");
            break;
        case NT_DIV:
            printf("  cqo\n");
            printf("  idiv rax, rdi\n");
            break;
        case NT_EQ:
            printf("  cmp rax, rdi\n");
            printf("  sete al\n");
            printf("  movzb rax, al\n");
            break;
        case NT_NE:
            printf("  cmp rax, rdi\n");
            printf("  setne al\n");
            printf("  movzb rax, al\n");
            break;
        case NT_LT:
            printf("  cmp rax, rdi\n");
            printf("  setl al\n");
            printf("  movzb rax, al\n");
            break;
        case NT_LE:
            printf("  cmp rax, rdi\n");
            printf("  setle al\n");
            printf("  movzb rax, al\n");
            break;
        default: panic("codegen: invalid node NodeTag=%d", node->tag);
    }
    printf("  push rax\n");
}

static void gen_stmt(Node *node, Var *env) {
    static int id = 0;
    int local_id;
    if (node->tag == NT_RETURN) {
        gen_expr(node->unary_expr, env);
        printf("  pop rax\n");
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
        return;
    } else if (node->tag == NT_BLOCK) {
        for (int i = 0; i < node->block->len; i++) {
            gen_stmt(node->block->nodes[i], env);
            printf("  pop rax\n");
        }
        return;
    } else if (node->tag == NT_IF) {
        local_id = id++;
        gen_expr(node->ifstmt.cond, env);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L%d.ELSE\n", local_id);
        gen_stmt(node->ifstmt.then, env);
        printf("  jmp .L%d.END\n", local_id);
        printf(".L%d.ELSE:\n", local_id);
        if (node->ifstmt.els != NULL) {
            gen_stmt(node->ifstmt.els, env);
        }
        printf(".L%d.END:\n", local_id);
        return;
    } else if (node->tag == NT_WHILE) {
        local_id = id++;
        printf(".L%d.WHILE:\n", local_id);
        gen_expr(node->whilestmt.cond, env);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L%d.END\n", local_id);
        gen_stmt(node->whilestmt.body, env);
        printf("  jmp .L%d.WHILE\n", local_id);
        printf(".L%d.END:\n", local_id);
        return;
    } else if (node->tag == NT_FOR) {
        local_id = id++;
        gen_expr(node->forstmt.def, env);
        printf(".L%d.FOR:\n", local_id);
        gen_expr(node->forstmt.cond, env);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L%d.END\n", local_id);
        gen_stmt(node->forstmt.body, env);
        gen_expr(node->forstmt.next, env);
        printf("  jmp .L%d.FOR\n", local_id);
        printf(".L%d.END:\n", local_id);
        return;
    }
    gen_expr(node, env);
}

void gen_func(Node *node) {
    Var *env = node->func.locals;
    int offset = env ? env->offset : 0;
    const char *name = node->func.name->main_token->start;
    int name_len = node->func.name->main_token->len;
    Node *body = node->func.body;

    printf(".globl %.*s\n", name_len, name);
    printf("%.*s:\n", name_len, name);
    // prologue
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", align_16(offset));

    // set args
    NodeList *params = node->func.params;
    int nparam = params->len;
    for (int i = 0; i < nparam; i++) {
        Node *node = params->nodes[i];
        Var *var = find_local_var(env, node->main_token);
        int offset = var->offset;
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", offset);
        printf("  push rax\n");
        printf("  mov [rax], %s\n", argreg[i]);
        // printf("  mov [rbp-%d], %s\n", offset, argreg[i]);
    }

    if (body->tag != NT_BLOCK) panic("codegen: expected block");
    gen_stmt(body, env);

    // epilogue
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

void gen(NodeList *nlist) {
    printf(".intel_syntax noprefix\n");
    for (int i = 0; i < nlist->len; i++) {
        Node *node = nlist->nodes[i];
        gen_func(node);
        printf("\n");
    }
}
