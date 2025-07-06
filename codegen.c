#include "kcc.h"

void print_token(Token *token) {
    const char *start = token->start;
    int len = token->len;
    printf("%.*s", len, start);
}

static void gen_local_var(Node *node, Var *env);
static void gen_fncall(Node *node, Var *env);
static void gen_expr(Node *node, Var *env);

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
    static char *argreg[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
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

static void gen_stmt(NodeList *nlist, Var *env) {
    for (int i = 0; i < nlist->len; i++) {
        Node *node = nlist->nodes[i];
        if (node->tag == NT_RETURN) {
            gen_expr(node->unary_expr, env);
            printf("  pop rax\n");
            printf("  mov rsp, rbp\n");
            printf("  pop rbp\n");
            printf("  ret\n");
            continue;
        }
        gen_expr(nlist->nodes[i], env);
        printf("  pop rax\n");
        printf("\n");
    }
}

void gen(NodeList *nlist, Var *env) {
    int offset = env ? env->offset : 0;
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", align_16(offset));
    gen_stmt(nlist, env);
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}
