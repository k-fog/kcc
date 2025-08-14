#include "kcc.h"

Preprocessor *preprocessor_new(const char *input) {
    Preprocessor *pp = calloc(1, sizeof(Preprocessor));
    pp->input = input;
    pp->pos = 0;
    pp->defines = NULL;
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
        Token *token_define = t->next;
        Token *token_from = token_define ? token_define->next : NULL;
        Token *token_to = token_from ? token_from->next : NULL;
        if (!token_define || !token_from || !token_to) panic("preprocess error");
        append_define(pp, token_from, token_to);
        prev->next = token_to->next;
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

