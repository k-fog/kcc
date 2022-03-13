#include "kcc.h"

// 入力プログラム
char *user_input;

int main(int argc, char **argv) {
    if(argc != 2) {
        error("invalid number of arguments.");
        return 1;
    }

    user_input = argv[1];
    token = tokenize(user_input);
    Node *node = expr();

    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");

    gen(node);
    
    printf("  pop rax\n");
    printf("  ret\n");
    return 0;
}

