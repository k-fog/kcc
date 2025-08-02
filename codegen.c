#include "kcc.h"


static char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char *argreg32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static char *str_label = ".L.STR";

static Node *current_func;

void print_token(Token *token) {
    const char *start = token->start;
    int len = token->len;
    printf("%.*s", len, start);
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
static void gen_expr_postfix(Node *node, Env *env);
static void gen_expr_assign(Node *node, Env *env);
static void gen_expr_binary(Node *node, Env *env);
static void gen_expr_cond(Node *node, Env *env);
static void gen_expr(Node *node, Env *env);
static void gen_lvardecl(Node *node, Env *env);
static void gen_stmt(Node *node, Env *env);
static void gen_func(Node *node, Env *env);

static void gen_globalvar(Symbol *var);

static int align_16(int n) {
    // return ((n + 15) / 16) * 16;
    return (n + 15) & ~0xF;
}

// load [rax] to rax
static void gen_load(Type *type) {
    switch (type->tag) {
        case TYP_VOID: panic("invalid load target: void");
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
        case TYP_VOID: panic("invalid store target: void");
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
        Symbol *var = find_symbol(ST_LVAR, env->local_vars, node->main_token);
        if (var != NULL) {
            int offset = var->offset;
            printf("  mov rax, rbp\n");
            printf("  sub rax, %d\n", offset);
            printf("  push rax\n");
            return;
        }
        var = find_symbol(ST_GVAR, env->global_vars, node->main_token);
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
    switch (node->tag) {
        case NT_NEG:
            gen_expr(node->unary_expr, env);
            printf("  pop rax\n");
            printf("  neg rax\n");
            printf("  push rax\n");
            break;
        case NT_ADDR:
            return gen_addr(node->unary_expr, env);
        case NT_DEREF:
            gen_expr(node->unary_expr, env);
            printf("  pop rax\n");
            gen_load(node->type);
            printf("  push rax\n");
            break;
        case NT_BOOL_NOT:
            gen_expr(node->unary_expr, env);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  sete al\n");
            printf("  movzx rax, al\n");
            printf("  push rax\n");
            break;
        case NT_SIZEOF: {
            int size = sizeof_type(node->unary_expr->type);
            printf("  push %d\n", size);
            break;
        }
        case NT_PREINC:
            gen_addr(node->unary_expr, env); // push addr, rax=address
            gen_load(node->type);
            if (is_integer(node->type)) printf("  inc rax\n");
            else if (is_ptr_or_arr(node->type)) printf("  add rax, %d\n", sizeof_type(node->type->base));
            gen_store(node->type); // pop addr
            printf("  push rax\n");
            break;
        case NT_PREDEC:
            gen_addr(node->unary_expr, env); // push addr, rax=address
            gen_load(node->type);
            if (is_integer(node->type)) printf("  dec rax\n");
            else if (is_ptr_or_arr(node->type)) printf("  sub rax, %d\n", sizeof_type(node->type->base));
            gen_store(node->type); // pop addr
            printf("  push rax\n");
            break;
        default: panic("codegen: error at gen_expr_unary");
    }
    return;
}

static void gen_expr_postfix(Node *node, Env *env) {
    if (node->tag == NT_POSTINC) {
        gen_addr(node->pre_expr, env); // push addr
        gen_load(node->type);
        printf(" mov rdx, rax\n");
        if (is_integer(node->type)) printf("  inc rax\n");
        else if (is_ptr_or_arr(node->type)) printf("  add rax, %d\n", sizeof_type(node->type->base));
        gen_store(node->type); // pop addr
        printf(" push rdx\n");
    } else if (node->tag == NT_POSTDEC) {
        gen_addr(node->pre_expr, env); // push addr
        gen_load(node->type);
        printf(" mov rdx, rax\n");
        if (is_integer(node->type)) printf("  dec rax\n");
        else if (is_ptr_or_arr(node->type)) printf("  sub rax, %d\n", sizeof_type(node->type->base));
        gen_store(node->type); // pop addr
        printf(" push rdx\n");
    } else {
        panic("codegen: error at gen_expr_postfix");
    }
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

    if (is_ptr_or_arr(lt) && is_integer(rt)) {
        // ptr +/- int
        if (node->tag != NT_ADD && node->tag != NT_SUB) panic("codegen: invalid operands");
        printf("  pop rdi\n");
        printf("  imul rdi, %d\n", sizeof_type(lt->base));
        printf("  pop rax\n");
    } else if (is_integer(lt) && is_ptr_or_arr(rt)) {
        // int + ptr
        if (node->tag != NT_ADD) panic("codegen: invalid operands");
        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  imul rax, %d\n", sizeof_type(rt->base));
    } else if (is_ptr_or_arr(lt) && is_ptr_or_arr(rt)) {
        // ptr - ptr
        if (node->tag != NT_SUB) panic("codegen: invalid operands");
        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  sub rax, rdi\n");
        printf("  sar rax, %d\n", 2);// sizeof_type(node->type));
        printf("  push rax\n");
        return;
    } else {
        // int op int 
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
        case NT_MOD:
            printf("  cqo\n");
            printf("  idiv rdi\n");
            if (node->tag == NT_MOD) printf("  mov rax, rdx\n");
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
        case NT_COMMA:
            printf("  mov rax, rdi\n");
            break;
        default: panic("codegen: invalid node NodeTag=%d", node->tag);
    }
    printf("  push rax\n");
}

static void gen_expr_cond(Node *node, Env *env) {
    int id = count();
    gen_expr(node->cond_expr.cond, env);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je  .L%d.ELSE\n", id);
    gen_expr(node->cond_expr.then, env);
    printf("  jmp .L%d.END\n", id);
    printf(".L%d.ELSE:\n", id);
    gen_expr(node->cond_expr.els, env);
    printf(".L%d.END:\n", id);
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
        case NT_PREINC:
        case NT_PREDEC:
        case NT_SIZEOF: return gen_expr_unary(node, env);
        case NT_POSTINC:
        case NT_POSTDEC: return gen_expr_postfix(node, env);
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
        case NT_MOD:
        case NT_EQ:
        case NT_NE:
        case NT_LT:
        case NT_LE:
        case NT_COMMA: return gen_expr_binary(node, env);
        case NT_COND: return gen_expr_cond(node, env);
        default: panic("codegen: error at gen_expr");
    }
}

static void gen_lvardecl(Node *node, Env *env) {
    NodeList *declarators = node->declarators;
    for (int i = 0; i < declarators->len; i++) {
        Node *child = declarators->nodes[i]; // declarator
        Node *name = child->declarator.name;
        Node *init = child->declarator.init;
        if (!init) continue;

        gen_addr(name, env);
        if (init->tag == NT_INITS) {
            NodeList *inits = init->initializers;
            printf("  mov rdx, rax\n");
            for (int i = 0; i < inits->len; i++) {
                Node *elem = inits->nodes[i]; // initializer
                gen_expr(elem, env);
                printf("  pop rax\n");
                gen_store(name->type->base);
                printf("  add rdx, %d\n", sizeof_type(name->type->base));
                printf("  push rdx\n");
            }
        } else {
            gen_expr(init, env);
            printf("  pop rax\n");
            gen_store(name->type);
        }
    }
}

static void gen_stmt(Node *node, Env *env) {
    int id = count();
    if (node->tag == NT_RETURN) {
        const char *name = current_func->func.name->main_token->start;
        int name_len = current_func->func.name->main_token->len;
        if (node->unary_expr) {
            gen_expr(node->unary_expr, env);
            printf("  pop rax\n");
        }
        printf("  jmp .L.RETURN.%.*s\n", name_len, name);
        return;
    } else if (node->tag == NT_BLOCK) {
        for (int i = 0; i < node->block->len; i++) {
            Node *child = node->block->nodes[i];
            gen_stmt(child, env);
            if (child->tag == NT_LOCALDECL || child->tag == NT_PARAMDECL) continue;
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
        if (node->forstmt.def) gen_expr(node->forstmt.def, env);
        printf(".L%d.FOR:\n", id);
        if (node->forstmt.cond) gen_expr(node->forstmt.cond, env);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L%d.END\n", id);
        if (node->forstmt.body) gen_stmt(node->forstmt.body, env);
        if (node->forstmt.next) gen_expr(node->forstmt.next, env);
        printf("  jmp .L%d.FOR\n", id);
        printf(".L%d.END:\n", id);
        return;
    } else if (node->tag == NT_LOCALDECL) return gen_lvardecl(node, env);
    else if (node->tag == NT_PARAMDECL) return;
    gen_expr(node, env);
}

static char *type2asm(Type *type) {
    switch (type->tag) {
        case TYP_CHAR:  return ".byte";
        case TYP_INT:   return ".long";
        case TYP_PTR:   return ".quad";
        case TYP_ARRAY: return type2asm(type->base);
        default: panic("codegen: error at type2asm");
    }
    return NULL;
}

static void gen_globalvar(Symbol *var) {
    printf(".data\n");
    printf("%.*s:\n", var->token->len, var->token->start);
    switch (var->type->tag) {
        case TYP_VOID: panic("codegen: error at gen_globalvar");
        case TYP_CHAR:
        case TYP_INT: {
            if (var->init && var->init->tag != NT_INT) panic("expression is not supported as initializers");
            int init_val = var->init ? var->init->integer : 0;
            printf("  %s %d\n", type2asm(var->type), init_val);
            break;
        }
        case TYP_PTR:
            if (!var->init)
                printf("  %s %d\n", type2asm(var->type->base), 0);
            else
                panic("unimplemented: global pointer initializer");
            break;
        case TYP_ARRAY: {
            if (!var->init) {
                printf("  .zero %d\n", sizeof_type(var->type));
                break;
            }
            NodeList *inits = var->init->initializers;
            for (int i = 0; i < inits->len; i++) {
                Node *init = inits->nodes[i];
                if (init->tag != NT_INT) panic("expression is not supported as initializers");
                int init_val = init ? init->integer : 0;
                printf("  %s %d\n", type2asm(var->type->base), init_val);
            }
        }
    }
}

static void gen_func(Node *node, Env *env) {
    current_func = node;
    int offset = env->local_vars ? env->local_vars->offset : 0;
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
        Symbol *var = find_symbol(ST_LVAR, env->local_vars, node->ident->main_token);
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

    // epilogue
    printf(".L.RETURN.%.*s:\n", name_len, name);
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

void gen(Program *prog) {
    printf(".intel_syntax noprefix\n\n");

    // generate strings
    for (int i = 0; i < prog->string_tokens->len; i++) {
        Token *token = prog->string_tokens->tokens[i];
        printf("%s%d:\n", str_label, i);
        printf("  .string %.*s\n\n", token->len, token->start);
    }

    // generate global variables
    for (Symbol *global = prog->global_vars; global != NULL; global = global->next) {
        gen_globalvar(global);
        printf("\n");
    }

    // generate functions
    Env *env = env_new(NULL, prog->global_vars, prog->func_types);
    for (int i = 0; i < prog->funcs->len; i++) {
        Node *fnode = prog->funcs->nodes[i];
        env->local_vars = fnode->func.locals; // set local variables
        gen_func(fnode, env);
        printf("\n");
    }
    free(env);
}
