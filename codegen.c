#include "kcc.h"

void gen_localvar(Node *node) {
    if (node->tag != NT_IDENT) panic("expected an identifier");
    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", 8 * (*node->main_token->start - 'a' + 1));
    printf("  push rax\n");
}

void gen(Node *node) {
    if (node->tag == NT_INT) {
        printf("  push %d\n", node->integer);
        return;
    } else if (node->tag == NT_NEG) {
        gen(node->unary_expr);
        printf("  pop rax\n");
        printf("  neg rax\n");
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_BOOL_NOT) {
        gen(node->unary_expr);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  sete al\n");
        printf("  movzx rax, al\n");
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_IDENT) {
        gen_localvar(node);
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_ASSIGN) {
        gen_localvar(node->expr.lhs);
        gen(node->expr.rhs);
        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  mov [rax], rdi\n");
        printf("  push rdi\n");
        return;

    }

    gen(node->expr.lhs);
    gen(node->expr.rhs);

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
