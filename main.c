#include "kcc.h"

void panic(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void dump_nodes(Node *node) {
    char buf[36];
    switch (node->tag) {
        case NT_INT:
            printf("%d ", node->integer);
            return;
        case NT_IDENT:
            strncpy(buf, node->main_token->start, node->main_token->len);
            buf[node->main_token->len] = '\0';
            printf("%s ", buf);
            return;
        case NT_NEG:
            printf("(- ");
            dump_nodes(node->unary_expr);
            break;
        case NT_BOOL_NOT:
            printf("(! ");
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
            if (node->tag == NT_ASSIGN) printf("(= ");
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

void dump_nodelist(NodeList *nlist) {
    for (int i = 0; i < nlist->len; i++) {
        dump_nodes(nlist->nodes[i]);
        printf("\n");
    }
}

void dump_locals(Parser *parser) {
    char buf[36];
    for (Var *var = parser->locals; var != NULL; var = var->next) {
        strncpy(buf, var->name, var->len);
        buf[var->len] = '\0';
        printf("name: %s\toffset:%d\n", buf, var->offset);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "invalid arg\n");
        return 1;
    }
    Lexer *lexer = lexer_new(argv[1]);
    Token *tokens = tokenize(lexer);
    // dump_tokens(tokens);
    Parser *parser = parser_new(tokens);
    NodeList *nlist = parse(parser);
    Var *env = get_local_vars(parser);
    // dump_nodelist(root);
    // dump_locals(parser);
    // return 0;
    gen(nlist, env);
    return 0;
}
