#include "kcc.h"

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
        default: panic("codegen: invalid node");
    }
    printf("  push rax\n");
}
