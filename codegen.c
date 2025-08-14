#include "kcc.h"


static char *argreg8[6];
static char *argreg32[6];
static char *argreg64[6];
static char *str_label;

void print_token(Token *token) {
    char *start = token->start;
    int len = token->len;
    printf("%.*s", len, start);
}

GenContext *gencontext_new(Node *current_func, Symbol *local_vars, Symbol *global_vars,
                           Symbol *func_types, Symbol *defined_types) {
    GenContext *ctx = calloc(1, sizeof(GenContext));
    ctx->current_func = current_func;
    ctx->local_vars = local_vars;
    ctx->global_vars = global_vars;
    ctx->func_types = func_types;
    ctx->defined_types = defined_types;
    ctx->break_id_top = 0;
    ctx->continue_id_top = 0;
    return ctx;
}

static void push_break_id(GenContext *ctx, int id) {
    int top = ctx->break_id_top;
    if (top == LOOP_STACK_SIZE) panic("internal error: too nested loop");
    ctx->break_id_stack[top] = id;
    (ctx->break_id_top)++;
    return;
}

static void pop_break_id(GenContext *ctx, int id) {
    (ctx->break_id_top)--;
    return;
}

static int current_break_id(GenContext *ctx) {
    int top = ctx->break_id_top;
    return ctx->break_id_stack[top - 1];
}

static void push_continue_id(GenContext *ctx, int id) {
    int top = ctx->continue_id_top;
    if (top == LOOP_STACK_SIZE) panic("internal error: too nested loop");
    ctx->continue_id_stack[top] = id;
    (ctx->continue_id_top)++;
    return;
}

static void pop_continue_id(GenContext *ctx, int id) {
    (ctx->continue_id_top)--;
    return;
}

static int current_continue_id(GenContext *ctx) {
    int top = ctx->continue_id_top;
    return ctx->continue_id_stack[top - 1];
}

int id_counter_global = 0;
static int count() {
    return id_counter_global++;
}

static void gen_load(Type *type);
static void gen_store(Type *type);
static void gen_addr(Node *node, GenContext *ctx);
static void gen_fncall(Node *node, GenContext *ctx);
static void gen_expr_unary(Node *node, GenContext *ctx);
static void gen_expr_postfix(Node *node, GenContext *ctx);
static void gen_expr_assign(Node *node, GenContext *ctx);
static void gen_expr_binary(Node *node, GenContext *ctx);
static void gen_expr_cond(Node *node, GenContext *ctx);
static void gen_expr_logical(Node *node, GenContext *ctx);
static void gen_expr(Node *node, GenContext *ctx);
static void gen_lvardecl(Node *node, GenContext *ctx);
static void gen_stmt(Node *node, GenContext *ctx);
static void gen_func(Node *node, GenContext *ctx);

static void gen_globalvar(Symbol *var);

// load [rax] to rax
static void gen_load(Type *type) {
    printf("  # gen_load\n");
    switch (type->tag) {
        case TYP_VOID: panic("invalid load target: void");
        case TYP_CHAR:
            printf("  movsx rax, byte ptr [rax]\n");
            break;
        case TYP_INT:
        case TYP_ENUM:
            printf("  movsxd rax, dword ptr [rax]\n");
            break;
        case TYP_PTR:
            printf("  mov rax, qword ptr [rax]\n");
            break;
        case TYP_ARRAY: return;
        case TYP_STRUCT: panic("invalid load target: struct");
        case TYP_UNION: panic("invalid load target: union");
    }
}

// store *ax to [stack top]
static void gen_store(Type *type) {
    printf("  # gen_store\n");
    printf("  pop rdi\n");
    switch (type->tag) {
        case TYP_VOID: panic("invalid store target: void");
        case TYP_CHAR:
            printf("  mov [rdi], al\n");
            break;
        case TYP_INT:
        case TYP_ENUM:
            printf("  mov [rdi], eax\n");
            break;
        case TYP_PTR:
            printf("  mov [rdi], rax\n");
            break;
        case TYP_ARRAY: panic("invalid store target: array");
        case TYP_STRUCT: panic("invalid store target: struct");
        case TYP_UNION: panic("invalid store target: struct");
    }
}

static void gen_addr(Node *node, GenContext *ctx) {
    printf("  # gen_addr\n");
    switch (node->tag) {
        case NT_IDENT: {
            Symbol *var = find_symbol(ST_LVAR, ctx->local_vars, node->main_token);
            Token *ident = node->main_token;
            printf("  # address of `%.*s`\n", ident->len, ident->start);
            if (var != NULL) {
                int offset = var->offset;
                printf("  mov rax, rbp\n");
                printf("  sub rax, %d\n", offset);
                printf("  push rax\n");
                return;
            }
            var = find_symbol(ST_GVAR, ctx->global_vars, node->main_token);
            if (!var) panic("undefined variable");
            printf("  lea rax, %.*s[rip]\n", var->token->len, var->token->start);
            printf("  push rax\n");
            break;
        }
        case NT_DEREF:
            gen_expr(node->unary_expr, ctx); // push rax
            break;
        case NT_STRING:
            printf("  lea rax, %s%d[rip]\n", str_label, node->index);
            printf("  push rax\n");
            break;
        case NT_DOT: {
            Node *lhs = node->member_access.lhs;
            Node *mnode = node->member_access.member;
            int offset = 0;
            Symbol *member = find_member(lhs->type->tagged_typ.list, mnode->main_token, &offset);
            if (!member) panic("wrong member");
            gen_addr(lhs, ctx);
            printf("  pop rax\n");
            printf("  add rax, %d\n", offset);
            printf("  push rax\n");
            break;
        }
        case NT_ARROW: {
            Node *lhs = node->member_access.lhs;
            Node *mnode = node->member_access.member;
            int offset = 0;
            Symbol *member = find_member(lhs->type->base->tagged_typ.list, mnode->main_token, &offset);
            if (!member) panic("wrong member");
            gen_expr(lhs, ctx);
            printf("  pop rax\n");
            printf("  add rax, %d\n", offset);
            printf("  push rax\n");
            break;
        }
        default:
            panic("unexpected node NodeTag=%d", node->tag);
    }
}

static void gen_fncall(Node *node, GenContext *ctx) {
    int id = count();
    Node **nodes = node->fncall.args->nodes;
    int narg = node->fncall.args->len;
    if (sizeof(argreg64) / sizeof(char*) < narg) panic("too many args");

    for (int i = 0; i < narg; i++) gen_expr(nodes[i], ctx);
    for (int i = narg - 1; 0 <= i; i--) {
        printf("  pop rax\n");
        printf("  mov %s, rax\n", argreg64[i]);
    }
    printf("  mov rax, rsp\n");
    printf("  and rax, 0xF\n");
    printf("  cmp rax, 0\n");
    printf("  je  .L.FNCALL%d.ALIGNED\n", id);
    printf("  sub rsp, 8\n");
    printf("  mov al, 0\n");
    printf("  call ");
    print_token(node->main_token);
    printf("\n");
    printf("  add rsp, 8\n");
    printf("  jmp .L.FNCALL%d.END\n", id);
    printf(".L.FNCALL%d.ALIGNED:\n", id);
    printf("  mov al, 0\n");
    printf("  call ");
    print_token(node->main_token);
    printf("\n");
    printf(".L.FNCALL%d.END:\n", id);
    printf("  push rax\n");
}

static void gen_expr_unary(Node *node, GenContext *ctx) {
    switch (node->tag) {
        case NT_NEG:
            gen_expr(node->unary_expr, ctx);
            printf("  pop rax\n");
            printf("  neg rax\n");
            printf("  push rax\n");
            break;
        case NT_ADDR:
            return gen_addr(node->unary_expr, ctx);
        case NT_DEREF:
            gen_expr(node->unary_expr, ctx);
            printf("  pop rax\n");
            gen_load(node->type);
            printf("  push rax\n");
            break;
        case NT_BOOL_NOT:
            gen_expr(node->unary_expr, ctx);
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
            gen_addr(node->unary_expr, ctx); // push addr, rax=address
            gen_load(node->type);
            if (is_integer(node->type)) printf("  inc rax\n");
            else if (is_ptr_or_arr(node->type)) printf("  add rax, %d\n", sizeof_type(node->type->base));
            gen_store(node->type); // pop addr
            printf("  push rax\n");
            break;
        case NT_PREDEC:
            gen_addr(node->unary_expr, ctx); // push addr, rax=address
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

static void gen_expr_postfix(Node *node, GenContext *ctx) {
    if (node->tag == NT_POSTINC) {
        gen_addr(node->pre_expr, ctx); // push addr
        gen_load(node->type);
        printf(" mov rdx, rax\n");
        if (is_integer(node->type)) printf("  inc rax\n");
        else if (is_ptr_or_arr(node->type)) printf("  add rax, %d\n", sizeof_type(node->type->base));
        gen_store(node->type); // pop addr
        printf(" push rdx\n");
    } else if (node->tag == NT_POSTDEC) {
        gen_addr(node->pre_expr, ctx); // push addr
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

static void gen_expr_assign(Node *node, GenContext *ctx) {
    printf("  # gen_expr_assign\n");
    gen_addr(node->expr.lhs, ctx);
    gen_expr(node->expr.rhs, ctx);
    if (node->tag == NT_ASSIGN) {
        printf("  pop rax\n");
    } else {
        Type *lt = node->expr.lhs->type;
        Type *rt = node->expr.rhs->type;

        if (is_ptr_or_arr(lt) && is_integer(rt)) {
            // ptr +=/-= int
            // if (node->tag != NT_ASSIGN_ADD && node->tag != NT_ASSIGN_SUB)
            //     panic("codegen: invalid operands (ptr op ptr)");
            printf("  pop rdi\n");
            printf("  imul rdi, %d\n", sizeof_type(lt->base));
            printf("  pop rsi\n");
        } else {
            printf("  pop rdi\n");
            printf("  pop rsi\n");
        }
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

static void gen_expr_binary(Node *node, GenContext *ctx) {
    gen_expr(node->expr.lhs, ctx);
    gen_expr(node->expr.rhs, ctx);

    Type *lt = node->expr.lhs->type;
    Type *rt = node->expr.rhs->type;

    if (is_ptr_or_arr(lt) && is_integer(rt)) {
        // ptr +/- int
        if (node->tag != NT_ADD && node->tag != NT_SUB
            && node->tag != NT_EQ && node->tag != NT_NE)
            panic("codegen: invalid operands (pointer op int)");
        printf("  pop rdi\n");
        printf("  imul rdi, %d\n", sizeof_type(lt->base));
        printf("  pop rax\n");
    } else if (is_integer(lt) && is_ptr_or_arr(rt)) {
        // int + ptr
        if (node->tag != NT_ADD) panic("codegen: invalid operands (int op pointer)");
        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  imul rax, %d\n", sizeof_type(rt->base));
    } else if (is_ptr_or_arr(lt) && is_ptr_or_arr(rt)) {
        // ptr - ptr
        if (node->tag == NT_SUB) {
            printf("  pop rdi\n");
            printf("  pop rax\n");
            printf("  sub rax, rdi\n");
            printf("  mov rsi, %d\n", sizeof_type(lt->base));
            printf("  cqo\n");
            printf("  idiv rsi\n");
            printf("  push rax\n");
            return;
        } else {
            // ptr op ptr
            // if (node->tag != NT_EQ && node->tag != NT_NE
            //     && node->tag != NT_LT && node->tag != NT_LE)
            //     panic("codegen: invalid operands (pointer op pointer)");
            printf("  pop rdi\n");
            printf("  pop rax\n");
        }
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

static void gen_expr_cond(Node *node, GenContext *ctx) {
    int id = count();
    gen_expr(node->cond_expr.cond, ctx);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je  .L%d.ELSE\n", id);
    gen_expr(node->cond_expr.then, ctx);
    printf("  jmp .L%d.END\n", id);
    printf(".L%d.ELSE:\n", id);
    gen_expr(node->cond_expr.els, ctx);
    printf(".L%d.END:\n", id);
}

static void gen_expr_logical(Node *node, GenContext *ctx) {
    int id = count();
    if (node->tag == NT_AND) {
        gen_expr(node->expr.lhs, ctx);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L%d.END\n", id);
        gen_expr(node->expr.rhs, ctx);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf(".L%d.END:\n", id);
        printf("  setne al\n");
        printf("  movzb rax, al\n");
        printf("  push rax\n");
    } else if (node->tag == NT_OR) {
        gen_expr(node->expr.lhs, ctx);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  jne .L%d.END\n", id);
        gen_expr(node->expr.rhs, ctx);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf(".L%d.END:\n", id);
        printf("  setne al\n");
        printf("  movzb rax, al\n");
        printf("  push rax\n");
    } else panic("codegen: error at gen_expr_logical");
}

static void gen_expr(Node *node, GenContext *ctx) {
    Token *token = node->main_token;
    printf("  # gen_expr: %.*s\n", token->len, token->start);
    switch (node->tag) {
        case NT_INT:
            printf("  push %d\n", node->integer);
            return;
        case NT_IDENT: {
            Symbol *mem = find_enum_val(ctx->defined_types, node->main_token);
            if (mem) {
                printf("  push %d\n", mem->value);
                return;
            }
            // fallthrough
        }
        case NT_DOT:
        case NT_ARROW:
            gen_addr(node, ctx);
            printf("  pop rax\n");
            gen_load(node->type);
            printf("  push rax\n");
            return;
        case NT_STRING: return gen_addr(node, ctx);
        case NT_NEG:
        case NT_ADDR:
        case NT_DEREF:
        case NT_BOOL_NOT:
        case NT_PREINC:
        case NT_PREDEC:
        case NT_SIZEOF: return gen_expr_unary(node, ctx);
        case NT_POSTINC:
        case NT_POSTDEC: return gen_expr_postfix(node, ctx);
        case NT_ASSIGN:
        case NT_ASSIGN_ADD:
        case NT_ASSIGN_SUB:
        case NT_ASSIGN_MUL:
        case NT_ASSIGN_DIV: return gen_expr_assign(node, ctx);
        case NT_FNCALL: return gen_fncall(node, ctx);
        case NT_ADD:
        case NT_SUB:
        case NT_MUL:
        case NT_DIV:
        case NT_MOD:
        case NT_EQ:
        case NT_NE:
        case NT_LT:
        case NT_LE:
        case NT_COMMA: return gen_expr_binary(node, ctx);
        case NT_COND: return gen_expr_cond(node, ctx);
        case NT_AND:
        case NT_OR: return gen_expr_logical(node, ctx);
        default: panic("codegen: error at gen_expr");
    }
}

static void gen_lvardecl(Node *node, GenContext *ctx) {
    NodeList *declarators = node->declarators;
    for (int i = 0; i < declarators->len; i++) {
        Node *child = declarators->nodes[i]; // declarator
        Node *name = child->declarator.name;
        Node *init = child->declarator.init;
        if (!init) continue;

        gen_addr(name, ctx);
        if (init->tag == NT_INITS) {
            NodeList *inits = init->initializers;
            printf("  mov rdx, rax\n");
            for (int i = 0; i < inits->len; i++) {
                Node *elem = inits->nodes[i]; // initializer
                gen_expr(elem, ctx);
                printf("  pop rax\n");
                gen_store(name->type->base);
                printf("  add rdx, %d\n", sizeof_type(name->type->base));
                printf("  push rdx\n");
            }
        } else {
            gen_expr(init, ctx);
            printf("  pop rax\n");
            gen_store(name->type);
        }
    }
}

static void gen_stmt(Node *node, GenContext *ctx) {
    if (!node) return;
    printf("  # gen_stmt\n");
    int id = count();
    if (node->tag == NT_RETURN) {
        Node *fnode = ctx->current_func;
        char *name = fnode->func.name->main_token->start;
        int name_len = fnode->func.name->main_token->len;
        if (node->unary_expr) {
            gen_expr(node->unary_expr, ctx);
            printf("  pop rax\n");
        }
        printf("  jmp .L.RETURN.%.*s\n", name_len, name);
        return;
    } else if (node->tag == NT_BLOCK) {
        for (int i = 0; i < node->block->len; i++) {
            Node *child = node->block->nodes[i];
            gen_stmt(child, ctx);
        }
        return;
    } else if (node->tag == NT_IF) {
        gen_expr(node->ifstmt.cond, ctx);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L%d.ELSE\n", id);
        gen_stmt(node->ifstmt.then, ctx);
        printf("  jmp .L%d.END\n", id);
        printf(".L%d.ELSE:\n", id);
        gen_stmt(node->ifstmt.els, ctx);
        printf(".L%d.END:\n", id);
        return;
    } else if (node->tag == NT_WHILE) {
        push_break_id(ctx, id);
        push_continue_id(ctx, id);
        printf(".L%d.WHILE:\n", id);
        printf(".L%d.CONTINUE:\n", id);
        gen_expr(node->whilestmt.cond, ctx);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .L%d.END\n", id);
        gen_stmt(node->whilestmt.body, ctx);
        printf("  jmp .L%d.WHILE\n", id);
        printf(".L%d.END:\n", id);
        pop_break_id(ctx, id);
        pop_continue_id(ctx, id);
        return;
    } else if (node->tag == NT_DO_WHILE) {
        push_break_id(ctx, id);
        push_continue_id(ctx, id);
        printf(".L%d.DO:\n", id);
        gen_stmt(node->whilestmt.body, ctx);
        printf(".L%d.CONTINUE:\n", id);
        gen_expr(node->whilestmt.cond, ctx);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  jne .L%d.DO\n", id);
        printf(".L%d.END:\n", id);
        pop_break_id(ctx, id);
        pop_continue_id(ctx, id);
        return;
    } else if (node->tag == NT_CASE) {
        for (int i = 0; i < node->caseblock.stmts->len; i++) {
            Node *child = node->caseblock.stmts->nodes[i];
            gen_stmt(child, ctx);
        }
        return;
    } else if (node->tag == NT_SWITCH) {
        push_break_id(ctx, id);
        int default_id = -1;
        for (int i = 0; i < node->switchstmt.cases->len; i++) {
            Node *child = node->switchstmt.cases->nodes[i];
            if (child->caseblock.constant == NULL) {
                default_id = i;
                continue;
            }
            gen_expr(child->caseblock.constant, ctx); // push
            gen_addr(node->switchstmt.control, ctx);  // push
            printf("  pop rax\n");                    // pop
            gen_load(node->switchstmt.control->type); // -> rax
            printf("  pop rdi\n");                    // pop
            printf("  cmp rax, rdi\n");
            printf("  je .L%d.CASE%d\n", id, i);
        }
        if (0 <= default_id) {
            // default:
            printf("  jmp .L%d.CASE%d\n", id, default_id);
        }
        for (int i = 0; i < node->switchstmt.cases->len; i++) {
            Node *child = node->switchstmt.cases->nodes[i];
            printf(".L%d.CASE%d:\n", id, i);
            gen_stmt(child, ctx);
        }
        printf(".L%d.END:\n", id);
        pop_break_id(ctx, id);
        return;
    } else if (node->tag == NT_FOR) {
        push_break_id(ctx, id);
        push_continue_id(ctx, id);
        if (node->forstmt.def) {
            if (node->forstmt.def->tag == NT_LOCALDECL) gen_lvardecl(node->forstmt.def, ctx);
            else {
                gen_expr(node->forstmt.def, ctx);
                printf("  pop rax\n");
            }
        }
        printf(".L%d.FOR:\n", id);
        if (node->forstmt.cond) {
            gen_expr(node->forstmt.cond, ctx);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je  .L%d.END\n", id);
        }
        gen_stmt(node->forstmt.body, ctx);
        printf(".L%d.CONTINUE:\n", id);
        if (node->forstmt.next) {
            gen_expr(node->forstmt.next, ctx);
            printf("  pop rax\n");
        }
        printf("  jmp .L%d.FOR\n", id);
        printf(".L%d.END:\n", id);
        pop_break_id(ctx, id);
        pop_continue_id(ctx, id);
        return;
    } else if (node->tag == NT_BREAK) {
        int goto_id = current_break_id(ctx);
        printf("  jmp .L%d.END\n", goto_id);
        return;
    } else if (node->tag == NT_CONTINUE) {
        int goto_id = current_continue_id(ctx);
        printf("  jmp .L%d.CONTINUE\n", goto_id);
        return;
    } else if (node->tag == NT_LOCALDECL) return gen_lvardecl(node, ctx);
    else if (node->tag == NT_PARAMDECL) return;

    // expr statement
    gen_expr(node, ctx);
    printf("  pop rax\n");
}

static char *type2asm(Type *type) {
    switch (type->tag) {
        case TYP_CHAR:  return ".byte";
        case TYP_ENUM:
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
        case TYP_ENUM:
        case TYP_INT: {
            if (var->init && var->init->tag != NT_INT) panic("expression is not supported as initializers");
            int init_val = var->init ? var->init->integer : 0;
            printf("  %s %d\n", type2asm(var->type), init_val);
            break;
        }
        case TYP_PTR:
            if (!var->init)
                printf("  %s %d\n", type2asm(var->type), 0);
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
            break;
        }
        case TYP_STRUCT:
        case TYP_UNION:
            printf("  .zero %d\n", sizeof_type(var->type));
            break;
    }
}

static void gen_func(Node *node, GenContext *ctx) {
    int offset = ctx->local_vars ? ctx->local_vars->offset : 0;
    char *name = node->func.name->main_token->start;
    int name_len = node->func.name->main_token->len;
    Node *body = node->func.body;

    printf(".globl %.*s\n", name_len, name);
    printf(".text\n");
    printf("%.*s:\n", name_len, name);
    // prologue
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", align_n(offset, 16));

    // set args
    NodeList *params = node->func.params;
    int nparam = params->len;
    for (int i = 0; i < nparam; i++) {
        Node *node = params->nodes[i];
        Symbol *var = find_symbol(ST_LVAR, ctx->local_vars, node->ident->main_token);
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
    gen_stmt(body, ctx);

    // epilogue
    printf(".L.RETURN.%.*s:\n", name_len, name);
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

void gen(Program *prog) {
    argreg8[0] = "dil";
    argreg8[1] = "sil";
    argreg8[2] = "dl";
    argreg8[3] = "cl";
    argreg8[4] = "r8b";
    argreg8[5] = "r9b";
    
    argreg32[0] = "edi";
    argreg32[1] = "esi";
    argreg32[2] = "edx";
    argreg32[3] = "ecx";
    argreg32[4] = "r8d";
    argreg32[5] = "r9d";
    
    argreg64[0] = "rdi";
    argreg64[1] = "rsi";
    argreg64[2] = "rdx";
    argreg64[3] = "rcx";
    argreg64[4] = "r8";
    argreg64[5] = "r9";
    str_label = ".L.STR";

    printf("# COMPILED BY %s\n", COMPILER_NAME);
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
    GenContext *ctx = gencontext_new(NULL, NULL, prog->global_vars,
                                     prog->func_types, prog->defined_types);
    for (int i = 0; i < prog->funcs->len; i++) {
        Node *fnode = prog->funcs->nodes[i];
        ctx->current_func = fnode;
        ctx->local_vars = fnode->func.locals; // set local variables
        gen_func(fnode, ctx);
        printf("\n");
    }
    free(ctx);
}
