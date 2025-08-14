#include "kcc.h"

#ifdef __STDC__
void panic(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}
#endif

#ifndef __STDC__
void panic() {
    exit(1);
}
#endif

char *read_file(char *path) {
    FILE *fp = stdin;
    if (strcmp(path, "-") != 0) {
        fp = fopen(path, "r");
        if (!fp) panic("cannot open file");
    }

    int capacity = 2048;
    int len = 0;
    char *buf = malloc(capacity);
    if (!buf) panic("cannot allocate memory");

    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (capacity <= len + 2) { // +2: \n\0
            capacity *= 2;
            char *tmp = realloc(buf, capacity);
            if (!tmp) panic("cannot reallocate memory");
            buf = tmp;
        }
        // buf[len++] = (char)c;
        buf[len++] = c;
    }
    buf[len++] = '\n';
    buf[len++] = '\0';

    if (fp != stdin) fclose(fp);
    return buf;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "invalid arg\n");
        return 1;
    }

    type_void = calloc(1, sizeof(Type));
    type_void->tag = TYP_VOID;
    type_int = calloc(1, sizeof(Type));
    type_int->tag = TYP_INT;
    type_char= calloc(1, sizeof(Type));
    type_char->tag = TYP_CHAR;

    char *src = read_file(argv[1]);
    Preprocessor *pp = preprocessor_new(src, NULL);
    Token *tokens = preprocess(pp);
    Parser *parser = parser_new(tokens);
    Program *prog = parse(parser);
    type_funcs(prog);
    gen(prog);
    return 0;
}
