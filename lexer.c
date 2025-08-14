#include "kcc.h"

struct {
    char *str; TokenTag tag;
} keywords[28];

Lexer *lexer_new(char *input) {
    Lexer *lexer = calloc(1, sizeof(Lexer));
    lexer->input = input;
    lexer->pos = 0;
    keywords[0].str = "return";   keywords[0].tag = TT_KW_RETURN;
    keywords[1].str = "if";       keywords[1].tag = TT_KW_IF;
    keywords[2].str = "else";     keywords[2].tag = TT_KW_ELSE;
    keywords[3].str = "while";    keywords[3].tag = TT_KW_WHILE;
    keywords[4].str = "for";      keywords[4].tag = TT_KW_FOR;
    keywords[5].str = "void";     keywords[5].tag = TT_KW_VOID;
    keywords[6].str = "int";      keywords[6].tag = TT_KW_INT;
    keywords[7].str = "char";     keywords[7].tag = TT_KW_CHAR;
    keywords[8].str = "sizeof";   keywords[8].tag = TT_KW_SIZEOF;
    keywords[9].str = "struct";   keywords[9].tag = TT_KW_STRUCT;
    keywords[10].str = "const";   keywords[10].tag = TT_KW_CONST;
    keywords[11].str = "static";  keywords[11].tag = TT_KW_STATIC;
    keywords[12].str = "extern";  keywords[12].tag = TT_KW_EXTERN;
    keywords[13].str = "break";   keywords[13].tag = TT_KW_BREAK;
    keywords[14].str = "continue";keywords[14].tag = TT_KW_CONTINUE;
    keywords[15].str = "do";      keywords[15].tag = TT_KW_DO;
    keywords[16].str = "switch";  keywords[16].tag = TT_KW_SWITCH;
    keywords[17].str = "case";    keywords[17].tag = TT_KW_CASE;
    keywords[18].str = "default"; keywords[18].tag = TT_KW_DEFAULT;
    keywords[19].str = "union";   keywords[19].tag = TT_KW_UNION;
    keywords[20].str = "enum";    keywords[20].tag = TT_KW_ENUM;
    keywords[21].str = "typedef"; keywords[21].tag = TT_KW_TYPEDEF;
    keywords[22].str = "define";  keywords[22].tag = TT_PP_DEFINE;
    keywords[23].str = "include"; keywords[23].tag = TT_PP_INCLUDE;
    keywords[24].str = "ifdef";   keywords[24].tag = TT_PP_IFDEF;
    keywords[25].str = "ifndef";  keywords[25].tag = TT_PP_IFNDEF;
    keywords[26].str = "endif";   keywords[26].tag = TT_PP_ENDIF;
    keywords[27].str = NULL;      keywords[27].tag = -1;
    return lexer;
}

static Token *token_new(TokenTag tag, char *start, int len) {
    Token *t = calloc(1, sizeof(Token));
    t->tag = tag;
    t->start = start;
    t->len = len;
    t->next = NULL;
    return t;
}

static char peek_tok(Lexer *lexer) {
    return lexer->input[lexer->pos];
}

static char *consume_tok(Lexer *lexer) {
    return &(lexer->input[(lexer->pos)++]);
}

static void skip_space(Lexer *lexer) {
    while (isspace(peek_tok(lexer))) consume_tok(lexer);
}

// if the given token matches a keyword, return its TokenTag.
// otherwise, return TT_IDENT.
static TokenTag lookup_ident(char *str, int len) {
    for (int i = 0; keywords[i].str != NULL; i++) {
        if (strlen(keywords[i].str) != len) continue;
        else if (strncmp(str, keywords[i].str, len) == 0) return keywords[i].tag;
    }
    return TT_IDENT;
}

Token *tokenize(Lexer *lexer) {
    Token head, *token = &head;
    while (token->tag != TT_EOF) {
        skip_space(lexer);
        char *start = consume_tok(lexer);
        char *end = start;

        switch (*start) {
            case '\0':
                token->next = token_new(TT_EOF, start, 1);
                break;
            case '#':
                token->next = token_new(TT_HASH, start, 1);
                break;
            case '+':
                if (peek_tok(lexer) == '+') {
                    consume_tok(lexer);
                    token->next = token_new(TT_PLUS_PLUS, start, 2);
                } else if (peek_tok(lexer) == '=') {
                    consume_tok(lexer);
                    token->next = token_new(TT_PLUS_EQ, start, 2);
                } else {
                    token->next = token_new(TT_PLUS, start, 1);
                }
                break;
            case '-':
                if (peek_tok(lexer) == '-') {
                    consume_tok(lexer);
                    token->next = token_new(TT_MINUS_MINUS, start, 2);
                } else if (peek_tok(lexer) == '=') {
                    consume_tok(lexer);
                    token->next = token_new(TT_MINUS_EQ, start, 2);
                } else if (peek_tok(lexer) == '>') {
                    consume_tok(lexer);
                    token->next = token_new(TT_MINUS_ANGLE_R, start, 2);
                } else {
                    token->next = token_new(TT_MINUS, start, 1);
                }
                break;
            case '*':
                if (peek_tok(lexer) == '=') {
                    consume_tok(lexer);
                    token->next = token_new(TT_STAR_EQ, start, 2);
                } else {
                    token->next = token_new(TT_STAR, start, 1);
                }
                break;
            case '/':
                if (peek_tok(lexer) == '/') {
                    while (peek_tok(lexer) != '\n' && peek_tok(lexer) != '\0') consume_tok(lexer);
                    continue;
                } else if (peek_tok(lexer) == '*') {
                    char *q = strstr(consume_tok(lexer), "*/");
                    if (!q) panic("\'*/\' not found");
                    lexer->pos = q - lexer->input + 2;
                    continue;
                } else if (peek_tok(lexer) == '=') {
                    consume_tok(lexer);
                    token->next = token_new(TT_SLASH_EQ, start, 2);
                } else {
                    token->next = token_new(TT_SLASH, start, 1);
                }
                break;
            case '(':
                token->next = token_new(TT_PAREN_L, start, 1);
                break;
            case ')':
                token->next = token_new(TT_PAREN_R, start, 1);
                break;
            case '{':
                token->next = token_new(TT_BRACE_L, start, 1);
                break;
            case '}':
                token->next = token_new(TT_BRACE_R, start, 1);
                break;
            case '[':
                token->next = token_new(TT_BRACKET_L, start, 1);
                break;
            case ']':
                token->next = token_new(TT_BRACKET_R, start, 1);
                break;
            case '%':
                token->next = token_new(TT_PERCENT, start, 1);
                break;
            case '=':
                if (peek_tok(lexer) != '=') {
                    token->next = token_new(TT_EQ, start, 1);
                } else {
                    consume_tok(lexer);
                    token->next = token_new(TT_EQ_EQ, start, 2);
                }
                break;
            case '!':
                if (peek_tok(lexer) != '=') {
                    token->next = token_new(TT_BANG, start, 1);
                } else {
                    consume_tok(lexer);
                    token->next = token_new(TT_BANG_EQ, start, 2);
                }
                break;
            case '?':
                token->next = token_new(TT_QUESTION, start, 1);
                break;
            case '<':
                if (peek_tok(lexer) != '=') {
                    token->next = token_new(TT_ANGLE_L, start, 1);
                } else {
                    consume_tok(lexer);
                    token->next = token_new(TT_ANGLE_L_EQ, start, 2);
                }
                break;
            case '>':
                if (peek_tok(lexer) != '=') {
                    token->next = token_new(TT_ANGLE_R, start, 1);
                } else {
                    consume_tok(lexer);
                    token->next = token_new(TT_ANGLE_R_EQ, start, 2);
                }
                break;
            case ':':
                token->next = token_new(TT_COLON, start, 1);
                break;
            case ';':
                token->next = token_new(TT_SEMICOLON, start, 1);
                break;
            case ',':
                token->next = token_new(TT_COMMA, start, 1);
                break;
            case '&':
                if (peek_tok(lexer) != '&') {
                    token->next = token_new(TT_AMPERSAND, start, 1);
                } else {
                    consume_tok(lexer);
                    token->next = token_new(TT_AND_AND, start, 1);
                }
                break;
            case '|':
                if (peek_tok(lexer) != '|') {
                    panic("unimplemented: |");
                } else {
                    consume_tok(lexer);
                    token->next = token_new(TT_PIPE_PIPE, start, 1);
                }
                break;
            case '"': {
                while (peek_tok(lexer) != '"') {
                    end = consume_tok(lexer);
                    if (*end == '\\' && *(end + 1) == '\"') consume_tok(lexer);
                }
                consume_tok(lexer); // end "
                int len_1 = end - start + 2; // string literal's token contains double quotes
                token->next = token_new(TT_STRING, start, len_1);
                break;
            }
            case '\'': {
                if (peek_tok(lexer) == '\\') {
                    consume_tok(lexer); // consume_tok back slash
                    end = consume_tok(lexer);
                }
                while (peek_tok(lexer) != '\'') end = consume_tok(lexer);
                end = consume_tok(lexer); // end '
                token->next = token_new(TT_CHAR, start, end - start + 1);
                break;
            }
            case '.':
                token->next = token_new(TT_PERIOD, start, 1);
                break;
            default:
                if (isdigit(*start)) {
                    while (isdigit(peek_tok(lexer))) end = consume_tok(lexer);
                    token->next = token_new(TT_INT, start, end - start + 1);
                } else if (isalpha(*start) || *start == '_') {
                    while (isalnum(peek_tok(lexer)) || peek_tok(lexer) == '_') end = consume_tok(lexer);
                    int len_2 = end - start + 1;
                    TokenTag tag = lookup_ident(start, len_2); // TT_IDENT or TT_<keyword>
                    token->next = token_new(tag, start, len_2);
                } else {
                    panic("tokenize error: %c", *start);
                }
                break;
        }
        token = token->next;
    }
    return head.next;
}
