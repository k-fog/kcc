#include "kcc.h"

void panic(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void dump_nodes(Node *node) {
    switch (node->tag) {
        case NT_INT:
            printf("%d ", node->integer);
            return;
        case NT_NEG:
            printf("(- ");
            dump_nodes(node->unary_expr);
            break;
        default: // expr
            if (node->tag == NT_ADD) printf("(+ ");
            if (node->tag == NT_SUB) printf("(- ");
            if (node->tag == NT_MUL) printf("(* ");
            if (node->tag == NT_DIV) printf("(/ ");
            if (node->tag == NT_EQ) printf("(== ");
            if (node->tag == NT_NE) printf("(!= ");
            if (node->tag == NT_LT) printf("(< ");
            if (node->tag == NT_LE) printf("(<= ");
            dump_nodes(node->expr.lhs);
            dump_nodes(node->expr.rhs);
            break;
    }
    printf("\b) ");
}

void dump_tokens(Token *tokens) {
    char buf[36];
    for (Token *t = tokens; t != NULL; t = t->next) {
        strncpy(buf, t->start, t->len);
        buf[t->len] = '\0';
        printf("%s\tTokenTag=%d\n", buf, t->tag);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "invalid arg\n");
        return 1;
    }
    Lexer *lexer = lexer_new(argv[1]);
    Token *tokens = tokenize(lexer);
    dump_tokens(tokens);
    Parser *parser = parser_new(tokens);
    Node *root = parse(parser);
    dump_nodes(root);
    printf("\n");
    return 0;

    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");
    gen(root);
    printf("  pop rax\n");
    printf("  ret\n");
    return 0;
}
