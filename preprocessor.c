#include "kcc.h"

Preprocessor *preprocessor_new(char *input, Symbol *defines) {
    Preprocessor *pp = calloc(1, sizeof(Preprocessor));
    pp->input = input;
    pp->defines = defines;
    return pp;
}

static Symbol *append_define(Preprocessor *pp, Token *token, Token *pp_token) {
    Symbol **defs = &pp->defines;
    Symbol *symbol = calloc(1, sizeof(Symbol));
    symbol->tag = ST_DEFINE;
    symbol->token = token;
    symbol->pp_token = pp_token;
    symbol->next = *defs;
    *defs = symbol;
    return symbol;
}

static char *strndupl(char *str, int len) {
    char *buffer = malloc(len + 1);
    memcpy(buffer, str, len);
    buffer[len] = '\0';
    return buffer;
}

static Token *ident_stdc() {
    Token *token = calloc(1, sizeof(Token));
    token->tag = TT_IDENT;
    token->start = "__STDC__";
    token->len = 8;
    return token;
}

Token *preprocess(Preprocessor *pp) {
    Lexer *lexer = lexer_new(pp->input);
    Token *tokens = tokenize(lexer);
    Token head, *prev;
    head.next = tokens;
    // look up #define
    for (Token *t = &head; t != NULL; t = t->next) {
        if (t->tag != TT_HASH) {
            prev = t;
            continue;
        }
        Token *token_directive = t->next;
        if (token_directive->tag == TT_PP_DEFINE) {
            Token *token_from = token_directive ? token_directive->next : NULL;
            Token *token_to = token_from ? token_from->next : NULL;
            if (!token_from || !token_to) panic("preprocess error: #define");
            append_define(pp, token_from, token_to);
            prev->next = token_to->next;
            t = token_to;
        } else if (token_directive->tag == TT_PP_INCLUDE) {
            Token *token_file = token_directive->next;
            if (!token_file || token_file->tag != TT_STRING) panic("preprocess error: #include");
            char *path = strndupl(token_file->start + 1, token_file->len - 2);
            char *src = read_file(path);
            Preprocessor *pp2 = preprocessor_new(src, pp->defines);
            Token *tokens2 = preprocess(pp2);
            pp->defines = pp2->defines;
            if (tokens2->tag != TT_EOF) {
                prev->next = tokens2;
                while (tokens2->next && tokens2->next->tag != TT_EOF) tokens2 = tokens2->next;
                tokens2->next = token_file->next;
                prev = tokens2;
            }
            t = token_file;
        } else if (token_directive->tag == TT_PP_IFDEF) {
            Token *pp_token = token_directive->next;
            if (!pp_token || !tokeneq(pp_token, ident_stdc()))
                panic("#ifdef is only implemented for __STDC__");
            while (t->tag != TT_PP_ENDIF) t = t->next; // skip to #endif
            prev->next = t->next;
        } else if (token_directive->tag == TT_PP_IFNDEF) {
            Token *pp_token = token_directive->next;
            if (!pp_token || !tokeneq(pp_token, ident_stdc()))
                panic("#ifndef is only implemented for __STDC__");
            prev->next = pp_token->next;
            t = pp_token;
        } else if (token_directive->tag == TT_PP_ENDIF) {
            prev->next = token_directive->next; // skip
            t = token_directive;
        } else panic("preprocess error: expected define or include after '#'");
    }
    for (Token *t = head.next; t != NULL; t = t->next) {
        for (Symbol *sym = pp->defines; sym != NULL; sym = sym->next) {
            if (!tokeneq(t, sym->token)) continue;
            // replace token
            t->tag = sym->pp_token->tag;
            t->start = sym->pp_token->start;
            t->len = sym->pp_token->len;
            break;
        }
    }
    return head.next;
}

