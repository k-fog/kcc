#include "kcc.h"

int label_count = 0;
static char *argreg[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

// ノードが変数を指しているとき、
// 変数のアドレスをスタックにpush
void gen_lval(Node *node) {
    if(node->kind != ND_LVAR) error("The left-value is not a variable.");
    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", node->offset);
    printf("  push rax\n");
}

void gen(Node *node) {
    if(!node) return;
    switch(node->kind) {
        case ND_NUM:
            printf("  push %d\n", node->val);
            return;
        case ND_LVAR:
            gen_lval(node);
            printf("  pop rax\n");
            printf("  mov rax, [rax]\n");
            printf("  push rax\n");
            return;
        case ND_ASSIGN:
            gen_lval(node->lhs);
            gen(node->rhs);
            printf("  pop rdi\n");
            printf("  pop rax\n");
            printf("  mov [rax], rdi\n");
            printf("  push rdi\n");
            return;
        case ND_RETURN:
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  mov rsp, rbp\n");
            printf("  pop rbp\n");
            printf("  ret\n");
            return;
        case ND_IF:
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je .Lels%d\n", label_count);
            gen(node->then);
            printf("  jmp .Lend%d\n", label_count);
            printf(".Lels%d:\n", label_count);
            gen(node->els);
            printf(".Lend%d:\n", label_count);
            label_count++;
            return;
        case ND_FOR:
            gen(node->init);
            printf(".Lbegin%d:\n", label_count);
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je .Lend%d\n", label_count);
            gen(node->lhs);
            gen(node->inc);
            printf("  jmp .Lbegin%d\n", label_count);
            printf(".Lend%d:\n", label_count);
            label_count++;
            return;
        case ND_WHILE:
            printf(".Lbegin%d:\n", label_count);
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je .Lend%d\n", label_count);
            gen(node->then);
            printf("  jmp .Lbegin%d\n", label_count);
            printf(".Lend%d:\n", label_count);
            label_count++;
            return;
        case ND_BLOCK:
            while(node) {
                gen(node->body);
                node = node->next;
                printf("  pop rax\n");
            }
            return;
        case ND_FNCALL:
            gen(node->args);
            printf("  mov rax, 0\n"); // 正しいかは不明
            printf("  call %s\n", node->name);
            printf("  push rax\n");
            return;
        case ND_ARGS: {
            int cnt_args = 0;
            while(node) {
                cnt_args++;
                gen(node->body);
                node = node->next;
            }
            while(cnt_args > 0) {
                printf("  pop %s\n", argreg[--cnt_args]);
            }
            return;
        }
        default:
            break;
    }
    gen(node->lhs);
    gen(node->rhs);
    printf("  pop rdi\n");
    printf("  pop rax\n");
    switch(node->kind) {
        case ND_ADD:
            printf("  add rax, rdi\n");
            break;
        case ND_SUB:
            printf("  sub rax, rdi\n");
            break;
        case ND_MUL:
            printf("  imul rax, rdi\n");
            break;
        case ND_DIV:
            printf("  cqo\n");
            printf("  idiv rdi\n");
            break;
        case ND_EQ:
            printf("  cmp rax, rdi\n");
            printf("  sete al\n");
            printf("  movzb rax, al\n");
            break;
        case ND_NE:
            printf("  cmp rax, rdi\n");
            printf("  setne al\n");
            printf("  movzb rax, al\n");
            break;
        case ND_LT:
            printf("  cmp rax, rdi\n");
            printf("  setl al\n");
            printf("  movzb rax, al\n");
            break;
        case ND_LE:
            printf("  cmp rax, rdi\n");
            printf("  setle al\n");
            printf("  movzb rax, al\n");
            break;
        default:
            break;
    }

    printf("  push rax\n");
}


