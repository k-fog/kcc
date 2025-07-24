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
        case NT_STRING:
            print_token(node->main_token);
            printf(" ");
            return;
        case NT_NEG:
            printf("(- ");
            dump_nodes(node->unary_expr);
            break;
        case NT_ADDR:
            printf("(& ");
            dump_nodes(node->unary_expr);
            break;
        case NT_DEREF:
            printf("(* ");
            dump_nodes(node->unary_expr);
            break;
        case NT_BOOL_NOT:
            printf("(! ");
            dump_nodes(node->unary_expr);
            break;
        case NT_SIZEOF:
            printf("(sizeof ");
            dump_nodes(node->unary_expr);
            break;
        case NT_VARDECL:
            printf("(decl "); // temporary
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
            dump_nodes(node->fncall.name);
            for (int i = 0; i < node->fncall.args->len; i++) {
                dump_nodes(node->fncall.args->nodes[i]);
            }
            break;
        case NT_FUNC:
            printf("(function ");
            printf("(");
            dump_nodes(node->func.name);
            for (int i = 0; i < node->func.params->len; i++) {
                dump_nodes(node->func.params->nodes[i]);
            }
            printf("\b) ");
            dump_nodes(node->func.body);
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
            else if (node->tag == NT_ASSIGN_ADD) printf("(+= ");
            else if (node->tag == NT_ASSIGN_SUB) printf("(-= ");
            else if (node->tag == NT_ASSIGN_MUL) printf("(*= ");
            else if (node->tag == NT_ASSIGN_DIV) printf("(/= ");
            dump_nodes(node->expr.lhs);
            dump_nodes(node->expr.rhs);
            break;
    }
    char *typ_str;
    if (!node->type) typ_str = "NULL";
    else {
        switch (node->type->tag) {
            case TYP_CHAR: typ_str = "char"; break;
            case TYP_INT: typ_str = "int"; break;
            case TYP_PTR: typ_str = "ptr"; break;
            case TYP_ARRAY: typ_str = "arr"; break;
        }
    }
    printf("\b)->%s ", typ_str);
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

void dump_locals(Node *node_fn) {
    for (Symbol *var = node_fn->func.locals; var != NULL; var = var->next) {
        const char *start = var->token->start;
        int len = var->token->len;
        printf("name:%.*s\toffset:%d\ttype:integer", len, start, var->offset);
        for (Type *type = var->type; (type != type_int && type != type_char); type = type->base) {
            printf("*");
        }
        printf("\n");
    }
}

void dump_funcs(NodeList *funcs) {
    for (int i = 0; i < funcs->len; i++) {
        Node *func = funcs->nodes[i];
        dump_nodes(func);
        printf("\n");
        dump_locals(func);
    }
}

char *read_file(char *path) {
    FILE *fp = stdin;
    if (strcmp(path, "-") != 0) {
        fp = fopen(path, "r");
        if (!fp) panic("cannot open %s: %s", path, strerror(errno));
    }

    int capacity = 2048;
    int len = 0;
    char *buf = malloc(capacity);
    if (!buf) panic("cannot allocate memory: %s", strerror(errno));

    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (capacity <= len + 2) { // +2: \n\0
            capacity *= 2;
            char *tmp = realloc(buf, capacity);
            if (!tmp) panic("cannot reallocate memory: %s", strerror(errno));
            buf = tmp;
        }
        buf[len++] = (char)c;
    }
    buf[len++] = '\n';
    buf[len] = '\0';

    if (fp != stdin) fclose(fp);
    return buf;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "invalid arg\n");
        return 1;
    }

#ifdef DEBUG
    char *src = read_file(argv[1]);
    Lexer *lexer = lexer_new(src);
    Token *tokens = tokenize(lexer);
    dump_tokens(tokens);
    Parser *parser = parser_new(tokens);
    Program *prog = parse(parser);
    dump_funcs(prog->funcs);
    type_funcs(prog);
    gen(prog);
    return 0;
#else
    char *src = read_file(argv[1]);
    Lexer *lexer = lexer_new(src);
    Token *tokens = tokenize(lexer);
    Parser *parser = parser_new(tokens);
    Program *prog = parse(parser);
    type_funcs(prog);
    gen(prog);
    return 0;
#endif
}
