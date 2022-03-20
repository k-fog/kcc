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

void gen_funcdef(Node *node) {
    if(node->kind != ND_FNDEF) error("It's not function definition.");
    printf("%s:\n", node->name);

    // prologue
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    int alloc = node->locals_num * 8;
    printf("  sub rsp, %d\n", alloc);

    printf("  mov rax, rbp\n");
    for(int i = 0; i < node->args_num; i++) {
        printf("  sub rax, 8\n");
        printf("  mov [rax], %s\n", argreg[i]);
    }

    gen(node->body);

    // epilogue
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
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
            printf("  je .L.els%d\n", label_count);
            gen(node->then);
            printf("  jmp .L.end%d\n", label_count);
            printf(".L.els%d:\n", label_count);
            gen(node->els);
            printf(".L.end%d:\n", label_count);
            label_count++;
            return;
        case ND_FOR:
            gen(node->init);
            printf(".L.begin%d:\n", label_count);
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je .L.end%d\n", label_count);
            gen(node->lhs);
            gen(node->inc);
            printf("  jmp .L.begin%d\n", label_count);
            printf(".L.end%d:\n", label_count);
            label_count++;
            return;
        case ND_WHILE:
            printf(".L.begin%d:\n", label_count);
            gen(node->cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je .L.end%d\n", label_count);
            gen(node->then);
            printf("  jmp .L.begin%d\n", label_count);
            printf(".L.end%d:\n", label_count);
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
            printf("  mov rax, rsp\n");
            printf("  and rax, 15\n");
            printf("  jnz .L.call.%d\n", label_count);
            printf("  mov rax, 0\n");
            printf("  call %s\n", node->name);
            printf("  jmp .L.end.%d\n", label_count);
            printf(".L.call.%d:\n", label_count);
            printf("  sub rsp, 8\n");
            printf("  mov rax, 0\n");
            printf("  call %s\n", node->name);
            printf("  add rsp, 8\n");
            printf(".L.end.%d:\n", label_count);
            printf("  push rax\n");
            label_count++;
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


