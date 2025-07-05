#include "kcc.h"

void gen_local_var(Node *node, Var *env) {
    if (node->tag != NT_IDENT) panic("expected an identifier");
    Var *var = find_local_var(env, node->main_token);
    if (var == NULL) panic("undefined variable");
    int offset = var->offset;
    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", offset);
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

void gen(NodeList *nlist, Var *env) {
    int offset = env ? env->offset : 0;
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", offset);
    for (int i = 0; i < nlist->len; i++) {
        gen_expr(nlist->nodes[i], env);
        printf("  pop rax\n");
        printf("\n");
    }
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}
