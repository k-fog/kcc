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
        case NT_ADD:
            printf("(+ ");
            dump_nodes(node->expr.lhs);
            dump_nodes(node->expr.rhs);
            break;
        case NT_SUB:
            printf("(- ");
            dump_nodes(node->expr.lhs);
            dump_nodes(node->expr.rhs);
            break;
        case NT_MUL:
            printf("(* ");
            dump_nodes(node->expr.lhs);
            dump_nodes(node->expr.rhs);
            break;
        case NT_DIV:
            printf("(/ ");
            dump_nodes(node->expr.lhs);
            dump_nodes(node->expr.rhs);
            break;
    }
    printf("\b) ");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "invalid arg\n");
        return 1;
    }
    Lexer *lexer = lexer_new(argv[1]);
    Token *tokens = tokenize(lexer);
    Parser *parser = parser_new(tokens);
    Node *root = parse(parser);
    dump_nodes(root);
    printf("\n");
    return 0;
}
