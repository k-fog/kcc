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
        case NT_IDENT:
            print_token(node->main_token);
            printf(" ");
            return;
        case NT_NEG:
            printf("(- ");
            dump_nodes(node->unary_expr);
            break;
        case NT_BOOL_NOT:
            printf("(! ");
            dump_nodes(node->unary_expr);
            break;
        case NT_RETURN:
            printf("(return ");
            dump_nodes(node->unary_expr);
            break;
        case NT_BLOCK:
            printf("(block \n");
            for (int i = 0; i < node->block->len; i++) {
                dump_nodes(node->block->nodes[i]);
                printf("\n");
            }
            break;
        case NT_IF:
            printf("(if ");
            dump_nodes(node->ifstmt.cond);
            printf("\b ");
            dump_nodes(node->ifstmt.then);
            if (node->ifstmt.els != NULL) {
                printf("\b ");
                dump_nodes(node->ifstmt.els);
            }
            break;
        case NT_WHILE:
            printf("(while ");
            dump_nodes(node->whilestmt.cond);
            printf("\b ");
            dump_nodes(node->whilestmt.body);
            break;
        case NT_FOR:
            printf("(for ");
            dump_nodes(node->forstmt.def);
            printf("\b ");
            dump_nodes(node->forstmt.cond);
            printf("\b ");
            dump_nodes(node->forstmt.next);
            printf("\b ");
            dump_nodes(node->forstmt.body);
            break;
        case NT_FNCALL:
            printf("(");
            dump_nodes(node->fncall.ident);
            for (int i = 0; i < node->fncall.args->len; i++) {
                dump_nodes(node->fncall.args->nodes[i]);
            }
            break;
        default: // expr
            if (node->tag == NT_ADD) printf("(+ ");
            else if (node->tag == NT_SUB) printf("(- ");
            else if (node->tag == NT_MUL) printf("(* ");
            else if (node->tag == NT_DIV) printf("(/ ");
            else if (node->tag == NT_EQ) printf("(== ");
            else if (node->tag == NT_NE) printf("(!= ");
            else if (node->tag == NT_LT) printf("(< ");
            else if (node->tag == NT_LE) printf("(<= ");
            else if (node->tag == NT_ASSIGN) printf("(= ");
            dump_nodes(node->expr.lhs);
            dump_nodes(node->expr.rhs);
            break;
    }
    printf("\b) ");
}

void dump_tokens(Token *tokens) {
    for (Token *t = tokens; t != NULL; t = t->next) {
        print_token(t);
        printf("\tTokenTag=%d\n", t->tag);
    }
}

void dump_nodelist(NodeList *nlist) {
    for (int i = 0; i < nlist->len; i++) {
        dump_nodes(nlist->nodes[i]);
        printf("\n");
    }
}

void dump_locals(Parser *parser) {
    for (Var *var = parser->locals; var != NULL; var = var->next) {
        const char *start = var->name;
        int len = var->len;
        printf("name:%.*s\toffset:%d\n", len, start, var->offset);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "invalid arg\n");
        return 1;
    }

#ifdef DEBUG
    Lexer *lexer = lexer_new(argv[1]);
    Token *tokens = tokenize(lexer);
    dump_tokens(tokens);
    Parser *parser = parser_new(tokens);
    NodeList *nlist = parse(parser);
    Var *env = get_local_vars(parser);
    dump_nodelist(nlist);
    dump_locals(parser);
    gen(nlist, env);
    return 0;
#else
    Lexer *lexer = lexer_new(argv[1]);
    Token *tokens = tokenize(lexer);
    Parser *parser = parser_new(tokens);
    NodeList *nlist = parse(parser);
    Var *env = get_local_vars(parser);
    gen(nlist, env);
    return 0;
#endif
}
