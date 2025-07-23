#include "kcc.h"


static char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char *argreg32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static char *str_label = ".L.STR";

void print_token(Token *token) {
    const char *start = token->start;
    int len = token->len;
    printf("%.*s", len, start);
}

Env *env_new(Symbol *locals, Symbol *globals) {
    Env *env = calloc(1, sizeof(Env));
    env->locals = locals;
    env->globals = globals;
    return env;
}

static int count() {
    static int i = 0;
    return i++;
}

static void gen_load(Type *type);
static void gen_store(Type *type);
static void gen_addr(Node *node, Env *env);
static void gen_fncall(Node *node, Env *env);
static void gen_expr_unary(Node *node, Env *env);
static void gen_expr_assign(Node *node, Env *env);
static void gen_expr_binary(Node *node, Env *env);
static void gen_expr(Node *node, Env *env);
static void gen_stmt(Node *node, Env *env);
static void gen_func(Node *node, Symbol *globals);

static int align_16(int n) {
    // return ((n + 15) / 16) * 16;
    return (n + 15) & ~0xF;
}

// load [rax] to rax
static void gen_load(Type *type) {
    switch (type->tag) {
        case TYP_CHAR:
            printf("  movsx rax, byte ptr [rax]\n");
            break;
        case TYP_INT:
            printf("  movsxd rax, dword ptr [rax]\n");
            break;
        case TYP_PTR:
            printf("  mov rax, qword ptr [rax]\n");
            break;
        case TYP_ARRAY: return;
    }
}

// store *ax to [stack top]
static void gen_store(Type *type) {
    printf("  pop rdi\n");
    switch (type->tag) {
        case TYP_CHAR:
            printf("  mov [rdi], al\n");
            break;
        case TYP_INT:
            printf("  mov [rdi], eax\n");
            break;
        case TYP_PTR:
            printf("  mov [rdi], rax\n");
            break;
        case TYP_ARRAY: panic("invalid store target: array");
    }
}

static void gen_addr(Node *node, Env *env) {
    if (node->tag == NT_IDENT) {
        Symbol *var = find_symbol(ST_LVAR, env->locals, node->main_token);
        if (var != NULL) {
            int offset = var->offset;
            printf("  mov rax, rbp\n");
            printf("  sub rax, %d\n", offset);
            printf("  push rax\n");
            return;
        }
        var = find_symbol(ST_GVAR, env->globals, node->main_token);
        if (var == NULL) panic("undefined variable");
        printf("  lea rax, %.*s[rip]\n", var->token->len, var->token->start);
        printf("  push rax\n");
    } else if (node->tag == NT_DEREF) {
        gen_expr(node->unary_expr, env);
    } else if (node->tag == NT_STRING) {
        printf("  lea rax, %s%d[rip]\n", str_label, node->index);
        printf("  push rax\n");
    } else {
        panic("unexpected node NodeTag=%d", node->tag);
    }
}

static void gen_fncall(Node *node, Env *env) {
    int id = count();
    Node **nodes = node->fncall.args->nodes;
    int narg = node->fncall.args->len;
    if (sizeof(argreg64) / sizeof(char*) < narg) panic("too many args");

    for (int i = 0; i < narg; i++) gen_expr(nodes[i], env);
    for (int i = narg - 1; 0 <= i; i--) {
        printf("  pop rax\n");
        printf("  mov %s, rax\n", argreg64[i]);
    }
    printf("  mov rax, rsp\n");
    printf("  and rax, 0xF\n");
    printf("  cmp rax, 0\n");
    printf("  je  .L.FNCALL%d.ALIGNED\n", id);
    printf("  sub rsp, 8\n");
    printf("  call ");
    print_token(node->main_token);
    printf("\n");
    printf("  add rsp, 8\n");
    printf("  jmp .L.FNCALL%d.END\n", id);
    printf(".L.FNCALL%d.ALIGNED:\n", id);
    printf("  call ");
    print_token(node->main_token);
    printf("\n");
    printf(".L.FNCALL%d.END:\n", id);
    printf("  push rax\n");
}

static void gen_expr_unary(Node *node, Env *env) {
    if (node->tag == NT_NEG) {
        gen_expr(node->unary_expr, env);
        printf("  pop rax\n");
        printf("  neg rax\n");
        printf("  push rax\n");
    } else if (node->tag == NT_ADDR) {
        gen_addr(node->unary_expr, env);
    } else if (node->tag == NT_DEREF) {
        gen_expr(node->unary_expr, env);
        printf("  pop rax\n");
        gen_load(node->type);
        printf("  push rax\n");
    } else if (node->tag == NT_BOOL_NOT) {
        gen_expr(node->unary_expr, env);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  sete al\n");
        printf("  movzx rax, al\n");
        printf("  push rax\n");
    } else if (node->tag == NT_SIZEOF) {
        int size = sizeof_type(node->unary_expr->type);
        printf("  push %d\n", size);
    } else {
        panic("codegen: error at gen_expr_unary");
    }
    return;
}

static void gen_expr_assign(Node *node, Env *env) {
    gen_addr(node->expr.lhs, env);
    gen_expr(node->expr.rhs, env);
    if (node->tag == NT_ASSIGN) {
        printf("  pop rax\n");
    } else {
        printf("  pop rdi\n");
        printf("  pop rsi\n");
        printf("  mov rax, rsi\n");
        if (node->tag == NT_ASSIGN_ADD) {
            gen_load(node->type);
            printf("  add rax, rdi\n");
            printf("  push rsi\n");
        } else if (node->tag == NT_ASSIGN_SUB) {
            gen_load(node->type);
            printf("  sub rax, rdi\n");
            printf("  push rsi\n");
        } else if (node->tag == NT_ASSIGN_MUL) {
            gen_load(node->type);
            printf("  imul rax, rdi\n");
            printf("  push rsi\n");
        } else if (node->tag == NT_ASSIGN_DIV) {
            gen_load(node->type);
            printf("  cqo\n");
            printf("  idiv rdi\n");
            printf("  push rsi\n");
        } else {
            panic("codegen: error at gen_expr_assign");
        }
    }
    gen_store(node->type);
    printf("  push rax\n");
    return;
}

static void gen_expr_binary(Node *node, Env *env) {
    gen_expr(node->expr.lhs, env);
    gen_expr(node->expr.rhs, env);

    Type *lt = node->expr.lhs->type;
    Type *rt = node->expr.rhs->type;

    if (node->tag == NT_ADD) {
        if ((lt->tag == TYP_PTR && rt->tag == TYP_INT) || 
            (lt->tag == TYP_ARRAY && rt->tag == TYP_INT)) {
            // ptr + int
            printf("  pop rdi\n");
            printf("  imul rdi, %d\n", sizeof_type(lt->base));
            printf("  pop rax\n");
        } else if (lt->tag == TYP_INT && rt->tag == TYP_PTR) {
            // int + ptr
            printf("  pop rdi\n");
            printf("  pop rax\n");
            printf("  imul rax, %d\n", sizeof_type(rt->base));
        } else {
            // int + int 
            printf("  pop rdi\n");
            printf("  pop rax\n");
        }
    } else if (node->tag == NT_SUB) {
        if (lt->tag == TYP_PTR && rt->tag == TYP_INT) {
            // ptr - int
            printf("  pop rdi\n");
            printf("  imul rdi, %d\n", sizeof_type(lt->base));
            printf("  pop rax\n");
        } else if (lt->tag == TYP_PTR && rt->tag == TYP_PTR) {
            // ptr - ptr
            printf("  pop rdi\n");
            printf("  pop rax\n");
            printf("  sub rax, rdi\n");
            printf("  sar rax, %d\n", 2);// sizeof_type(node->type));
            printf("  push rax\n");
            return;
        } else {
            // int - int 
            printf("  pop rdi\n");
            printf("  pop rax\n");
        }
    } else {
        printf("  pop rdi\n");
        printf("  pop rax\n");
    }

    switch (node->tag) {
        case NT_ADD:
            printf("  add rax, rdi\n");
            break;
        case NT_SUB:
            printf("  sub rax, rdi\n");
            break;
        case NT_MUL:
            printf("  imul rax, rdi\n");
            break;
        case NT_DIV:
            printf("  cqo\n");
            printf("  idiv rdi\n");
            break;
        case NT_EQ:
            printf("  cmp rax, rdi\n");
            printf("  sete al\n");
            printf("  movzb rax, al\n");
            break;
        case NT_NE:
            printf("  cmp rax, rdi\n");
            printf("  setne al\n");
            printf("  movzb rax, al\n");
            break;
        case NT_LT:
            printf("  cmp rax, rdi\n");
            printf("  setl al\n");
            printf("  movzb rax, al\n");
            break;
        case NT_LE:
            printf("  cmp rax, rdi\n");
            printf("  setle al\n");
            printf("  movzb rax, al\n");
            break;
        default: panic("codegen: invalid node NodeTag=%d", node->tag);
    }
    printf("  push rax\n");
}

static void gen_expr(Node *node, Env *env) {
    switch (node->tag) {
        case NT_INT:
            printf("  push %d\n", node->integer);
            return;
        case NT_IDENT:
            gen_addr(node, env);
            printf("  pop rax\n");
            gen_load(node->type);
            printf("  push rax\n");
            return;
        case NT_STRING: return gen_addr(node, env);
        case NT_NEG:
        case NT_ADDR:
        case NT_DEREF:
        case NT_BOOL_NOT:
        case NT_SIZEOF: return gen_expr_unary(node, env);
        case NT_ASSIGN:
        case NT_ASSIGN_ADD:
        case NT_ASSIGN_SUB:
        case NT_ASSIGN_MUL:
        case NT_ASSIGN_DIV: return gen_expr_assign(node, env);
        case NT_FNCALL: return gen_fncall(node, env);
        case NT_ADD:
        case NT_SUB:
        case NT_MUL:
        case NT_DIV:
        case NT_EQ:
        case NT_NE:
        case NT_LT:
        case NT_LE: return gen_expr_binary(node, env);
        default: panic("codegen: error at gen_expr");
    }
}

static void gen_stmt(Node *node, Env *env) {
    int id = count();
    if (node->tag == NT_RETURN) {
        gen_expr(node->unary_expr, env);
        printf("  pop rax\n");
        // epilogue
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
        return;
    } else if (node->tag == NT_BLOCK) {
        for (int i = 0; i < node->block->len; i++) {
            Node *child = node->block->nodes[i];
            if (child->tag == NT_VARDECL) continue; // skip if node is a variable declaration
            gen_stmt(child, env);
            printf("  pop rax\n");
        }
        return;
    } else if (node->tag == NT_IF) {
        gen_expr(node->ifstmt.cond, env);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L%d.ELSE\n", id);
        gen_stmt(node->ifstmt.then, env);
        printf("  jmp .L%d.END\n", id);
        printf(".L%d.ELSE:\n", id);
        if (node->ifstmt.els != NULL) {
            gen_stmt(node->ifstmt.els, env);
        }
        printf(".L%d.END:\n", id);
        return;
    } else if (node->tag == NT_WHILE) {
        printf(".L%d.WHILE:\n", id);
        gen_expr(node->whilestmt.cond, env);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L%d.END\n", id);
        gen_stmt(node->whilestmt.body, env);
        printf("  jmp .L%d.WHILE\n", id);
        printf(".L%d.END:\n", id);
        return;
    } else if (node->tag == NT_FOR) {
        gen_expr(node->forstmt.def, env);
        printf(".L%d.FOR:\n", id);
        gen_expr(node->forstmt.cond, env);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L%d.END\n", id);
        gen_stmt(node->forstmt.body, env);
        gen_expr(node->forstmt.next, env);
        printf("  jmp .L%d.FOR\n", id);
        printf(".L%d.END:\n", id);
        return;
    } else if (node->tag == NT_VARDECL) {
        panic("unreachable");
    }
    gen_expr(node, env);
}

static void gen_func(Node *node, Symbol *globals) {
    Env *env = env_new(node->func.locals, globals);

    int offset = env->locals ? env->locals->offset : 0;
    const char *name = node->func.name->main_token->start;
    int name_len = node->func.name->main_token->len;
    Node *body = node->func.body;

    printf(".globl %.*s\n", name_len, name);
    printf(".text\n");
    printf("%.*s:\n", name_len, name);
    // prologue
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", align_16(offset));

    // set args
    NodeList *params = node->func.params;
    int nparam = params->len;
    for (int i = 0; i < nparam; i++) {
        Node *node = params->nodes[i];
        Symbol *var = find_symbol(ST_LVAR, env->locals, node->main_token);
        int offset = var->offset;
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", offset);
        printf("  push rax\n");
        if (var->type->tag == TYP_CHAR) printf("  mov [rax], %s\n", argreg8[i]);
        else if (var->type->tag == TYP_INT) printf("  mov [rax], %s\n", argreg32[i]);
        else printf("  mov [rax], %s\n", argreg64[i]);
        // printf("  mov [rbp-%d], %s\n", offset, argreg[i]);
    }

    if (body->tag != NT_BLOCK) panic("codegen: expected block");
    gen_stmt(body, env);
}

void gen(NodeList *nlist, Symbol *globals, TokenList *string_tokens) {
    printf(".intel_syntax noprefix\n\n");
    for (Symbol *global = globals; global != NULL; global = global->next) {
        if (global->tag != ST_GVAR) continue;
        printf(".data\n");
        printf("%.*s:\n", global->token->len, global->token->start);
        printf("  .zero %d\n\n", sizeof_type(global->type));
    }

    for (int i = 0; i < string_tokens->len; i++) {
        Token *token = string_tokens->tokens[i];
        printf("%s%d:\n", str_label, i);
        printf("  .string %.*s\n\n", token->len, token->start);
    }

    for (int i = 0; i < nlist->len; i++) {
        Node *node = nlist->nodes[i];
        gen_func(node, globals);
        printf("\n");
    }
}
