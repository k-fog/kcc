#include "kcc.h"


static char *argreg[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

void print_token(Token *token) {
    const char *start = token->start;
    int len = token->len;
    printf("%.*s", len, start);
}

static void gen_load(Type *type);
static void gen_store(Type *type);
static void gen_addr(Node *node, Var *env);
static void gen_fncall(Node *node, Var *env);
static void gen_expr(Node *node, Var *env);
static void gen_stmt(Node *node, Var *env);
static void gen_func(Node *node);

static int align_16(int n) {
    // return ((n + 15) / 16) * 16;
    return (n + 15) & ~0xF;
}

// load [rax] to rax
static void gen_load(Type *type) {
    if (type->tag == TYP_ARRAY) return;
    int size = sizeof_type(type);
    if (size == 4) printf("  movsxd rax, [rax]\n");
    else printf("  mov rax, [rax]\n");
}

// store *ax to [stack top]
static void gen_store(Type *type) {
    printf("  pop rdi\n");
    int size = sizeof_type(type);
    if (size == 4) printf("  mov [rdi], eax\n");
    else printf("  mov [rdi], rax\n");
}

static void gen_addr(Node *node, Var *env) {
    if (node->tag == NT_IDENT) {
        Var *var = find_local_var(env, node->main_token);
        if (var == NULL) panic("undefined variable");
        int offset = var->offset;
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", offset);
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_DEREF) {
        gen_expr(node->unary_expr, env);
        return;
    }
    panic("unexpected node NodeTag=%d", node->tag);
}

static void gen_fncall(Node *node, Var *env) {
    static int id = 0;
    int local_id = id++;
    Node **nodes = node->fncall.args->nodes;
    int narg = node->fncall.args->len;
    if (sizeof(argreg) / sizeof(char*) < narg) panic("too many args");

    for (int i = 0; i < narg; i++) gen_expr(nodes[i], env);
    for (int i = narg - 1; 0 <= i; i--) {
        printf("  pop rax\n");
        printf("  mov %s, rax\n", argreg[i]);
    }
    printf("  mov rax, rsp\n");
    printf("  and rax, 0xF\n");
    printf("  cmp rax, 0\n");
    printf("  je  .L.FNCALL%d.ALIGNED\n", local_id);
    printf("  sub rsp, 8\n");
    printf("  call ");
    print_token(node->main_token);
    printf("\n");
    printf("  add rsp, 8\n");
    printf("  jmp .L.FNCALL%d.END\n", local_id);
    printf(".L.FNCALL%d.ALIGNED:\n", local_id);
    printf("  call ");
    print_token(node->main_token);
    printf("\n");
    printf(".L.FNCALL%d.END:\n", local_id);
    printf("  push rax\n");
}

static void gen_expr(Node *node, Var *env) {
    if (node->tag == NT_INT) {
        printf("  push %d\n", node->integer);
        return;
    } else if (node->tag == NT_NEG) {
        gen_expr(node->unary_expr, env);
        printf("  pop rax\n");
        printf("  neg rax\n");
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_ADDR) {
        gen_addr(node->unary_expr, env);
        return;
    } else if (node->tag == NT_DEREF) {
        gen_expr(node->unary_expr, env);
        printf("  pop rax\n");
        gen_load(node->type);
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_BOOL_NOT) {
        gen_expr(node->unary_expr, env);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  sete al\n");
        printf("  movzx rax, al\n");
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_SIZEOF) {
        int size = sizeof_type(node->unary_expr->type);
        printf("  push %d\n", size);
        return;
    } else if (node->tag == NT_IDENT) {
        gen_addr(node, env);
        printf("  pop rax\n");
        gen_load(node->type);
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_ASSIGN) {
        gen_addr(node->expr.lhs, env);
        gen_expr(node->expr.rhs, env);
        printf("  pop rax\n");
        gen_store(node->type);
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_ASSIGN_ADD) {
        gen_addr(node->expr.lhs, env);
        gen_expr(node->expr.rhs, env);
        printf("  pop rdi\n");
        printf("  pop rsi\n");
        printf("  mov rax, rsi\n");
        gen_load(node->type);
        printf("  add rax, rdi\n");
        printf("  push rsi\n");
        gen_store(node->type);
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_ASSIGN_SUB) {
        gen_addr(node->expr.lhs, env);
        gen_expr(node->expr.rhs, env);
        printf("  pop rdi\n");
        printf("  pop rsi\n");
        printf("  mov rax, rsi\n");
        gen_load(node->type);
        printf("  sub rax, rdi\n");
        printf("  push rsi\n");
        gen_store(node->type);
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_ASSIGN_MUL) {
        gen_addr(node->expr.lhs, env);
        gen_expr(node->expr.rhs, env);
        printf("  pop rdi\n");
        printf("  pop rsi\n");
        printf("  mov rax, rsi\n");
        gen_load(node->type);
        printf("  imul rax, rdi\n");
        printf("  push rsi\n");
        gen_store(node->type);
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_ASSIGN_DIV) {
        gen_addr(node->expr.lhs, env);
        gen_expr(node->expr.rhs, env);
        printf("  pop rdi\n");
        printf("  pop rsi\n");
        printf("  mov rax, rsi\n");
        gen_load(node->type);
        printf("  cqo\n");
        printf("  idiv rdi\n");
        printf("  push rsi\n");
        gen_store(node->type);
        printf("  push rax\n");
        return;
    } else if (node->tag == NT_FNCALL) {
        gen_fncall(node, env);
        return;
    }

    gen_expr(node->expr.lhs, env);
    gen_expr(node->expr.rhs, env);

    if ((node->tag == NT_ADD || node->tag == NT_SUB) && 
            node->expr.lhs->type->tag == TYP_PTR && node->expr.rhs->type->tag == TYP_INT) {
        printf("  pop rdi\n");
        printf("  imul rdi, 4\n");
        printf("  pop rax\n");
    } else if ((node->tag == NT_ADD || node->tag == NT_SUB) && 
            node->expr.lhs->type->tag == TYP_INT && node->expr.rhs->type->tag == TYP_PTR) {
        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  imul rax, 4\n");
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

static void gen_stmt(Node *node, Var *env) {
    static int id = 0;
    int local_id;
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
        local_id = id++;
        gen_expr(node->ifstmt.cond, env);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L%d.ELSE\n", local_id);
        gen_stmt(node->ifstmt.then, env);
        printf("  jmp .L%d.END\n", local_id);
        printf(".L%d.ELSE:\n", local_id);
        if (node->ifstmt.els != NULL) {
            gen_stmt(node->ifstmt.els, env);
        }
        printf(".L%d.END:\n", local_id);
        return;
    } else if (node->tag == NT_WHILE) {
        local_id = id++;
        printf(".L%d.WHILE:\n", local_id);
        gen_expr(node->whilestmt.cond, env);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L%d.END\n", local_id);
        gen_stmt(node->whilestmt.body, env);
        printf("  jmp .L%d.WHILE\n", local_id);
        printf(".L%d.END:\n", local_id);
        return;
    } else if (node->tag == NT_FOR) {
        local_id = id++;
        gen_expr(node->forstmt.def, env);
        printf(".L%d.FOR:\n", local_id);
        gen_expr(node->forstmt.cond, env);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L%d.END\n", local_id);
        gen_stmt(node->forstmt.body, env);
        gen_expr(node->forstmt.next, env);
        printf("  jmp .L%d.FOR\n", local_id);
        printf(".L%d.END:\n", local_id);
        return;
    } else if (node->tag == NT_VARDECL) {
        panic("unreachable");
    }
    gen_expr(node, env);
}

static void gen_func(Node *node) {
    Var *env = node->func.locals;
    int offset = env ? env->offset : 0;
    const char *name = node->func.name->main_token->start;
    int name_len = node->func.name->main_token->len;
    Node *body = node->func.body;

    printf(".globl %.*s\n", name_len, name);
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
        Var *var = find_local_var(env, node->main_token);
        int offset = var->offset;
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", offset);
        printf("  push rax\n");
        printf("  mov [rax], %s\n", argreg[i]);
        // printf("  mov [rbp-%d], %s\n", offset, argreg[i]);
    }

    if (body->tag != NT_BLOCK) panic("codegen: expected block");
    gen_stmt(body, env);
}

void gen(NodeList *nlist) {
    printf(".intel_syntax noprefix\n");
    for (int i = 0; i < nlist->len; i++) {
        Node *node = nlist->nodes[i];
        gen_func(node);
        printf("\n");
    }
}
