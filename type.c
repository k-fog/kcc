#include "kcc.h"

Env *env_new(Symbol *local_vars, Symbol *global_vars, Symbol *func_types) {
    Env *env = calloc(1, sizeof(Env));
    env->local_vars = local_vars;
    env->global_vars = global_vars;
    env->func_types = func_types;
    return env;
}

Type *type_int = &(Type){TYP_INT, NULL};
Type *type_char = &(Type){TYP_CHAR, NULL};

Type *pointer_to(Type *base) {
    Type *ptr = calloc(1, sizeof(Type));
    ptr->tag = TYP_PTR;
    ptr->base = base;
    return ptr;
}

Type *array_of(Type *base, int size) {
    Type *arr = calloc(1, sizeof(Type));
    arr->tag = TYP_ARRAY;
    arr->base = base;
    arr->array_size = size;
    return arr;
}

int sizeof_type(Type *type) {
    switch (type->tag) {
        case TYP_CHAR: return 1;
        case TYP_INT: return 4;
        case TYP_PTR: return 8;
        case TYP_ARRAY: return sizeof_type(type->base) * type->array_size;
        default: panic("error at sizeof_type");
    }
    return 0;
}

bool is_integer(Type *type) {
    if (type->tag == TYP_CHAR) return true;
    else if (type->tag == TYP_INT) return true;
    else return false;
}

bool is_ptr_or_arr(Type *type) {
    return type->tag == TYP_PTR || type->tag == TYP_ARRAY;
}

static Type *promote_if_integer(Type *type) {
    if (is_integer(type)) return type_int;
    else return type;
}

static bool is_compatible(Type *a, Type *b) {
    if (a->tag == TYP_PTR && a->tag == TYP_PTR)
        return is_compatible(a->base, b->base);
    else return is_integer(a) && is_integer(b);
}

static Node *typed(Node *node, Env *env) {
    if (!node || node->type != NULL) return node;
    switch (node->tag) {
        case NT_INT:
            node->type = type_int;
            break;
        case NT_IDENT: {
            Symbol *var = find_symbol(ST_LVAR, env->local_vars, node->main_token);
            if (var == NULL) var = find_symbol(ST_GVAR, env->global_vars, node->main_token);
            if (var == NULL) {
                panic("undefined variable: %.*s", node->main_token->len, node->main_token->start);
            }
            node->type = var->type;
            break;
        }
        case NT_STRING:
            node->type = array_of(type_char, node->main_token->len - 2 + 1); // -2: '"' * 2, +1: '\0'
            break;
        case NT_ADD: {
            Type *lhs_typ = promote_if_integer(typed(node->expr.lhs, env)->type);
            Type *rhs_typ = promote_if_integer(typed(node->expr.rhs, env)->type);
            if (lhs_typ->tag == TYP_INT && rhs_typ->tag == TYP_INT) node->type = type_int;
            else if (lhs_typ->tag == TYP_PTR && rhs_typ->tag == TYP_INT) node->type = lhs_typ;
            else if (lhs_typ->tag == TYP_INT && rhs_typ->tag == TYP_PTR) node->type = rhs_typ;
            else if (lhs_typ->tag == TYP_ARRAY && rhs_typ->tag == TYP_INT) node->type = pointer_to(lhs_typ->base);
            else if (lhs_typ->tag == TYP_INT && rhs_typ->tag == TYP_ARRAY) node->type = pointer_to(rhs_typ->base);
            else panic("undefined: ptr + ptr");
            break;
        }
        case NT_SUB: {
            Type *lhs_typ = promote_if_integer(typed(node->expr.lhs, env)->type);
            Type *rhs_typ = promote_if_integer(typed(node->expr.rhs, env)->type);
            if (lhs_typ->tag == TYP_INT && rhs_typ->tag == TYP_INT) node->type = type_int;
            else if (lhs_typ->tag == TYP_PTR && rhs_typ->tag == TYP_INT) node->type = lhs_typ;
            else if (lhs_typ->tag == TYP_ARRAY && rhs_typ->tag == TYP_INT) node->type = pointer_to(lhs_typ->base);
            else if (lhs_typ->tag == TYP_PTR && rhs_typ->tag == TYP_PTR) node->type = type_int;
            else if (lhs_typ->tag == TYP_ARRAY && rhs_typ->tag == TYP_PTR) node->type = type_int;
            else panic("undefined: int - ptr");
            break;
        }
        case NT_MUL:
        case NT_DIV:
        case NT_EQ:
        case NT_NE:
        case NT_LT:
        case NT_LE: {
            typed(node->expr.lhs, env);
            typed(node->expr.rhs, env);
            node->type = type_int;
            break;
        }
        case NT_NEG:
        case NT_BOOL_NOT:
        case NT_PREINC:
        case NT_PREDEC:
            node->type = promote_if_integer(typed(node->unary_expr, env)->type);
            break;
        case NT_ADDR: {
            Type *base = typed(node->unary_expr, env)->type;
            node->type = pointer_to(base);
            break;
        }
        case NT_DEREF: {
            Type *unary_typ = typed(node->unary_expr, env)->type;
            if (unary_typ->tag != TYP_PTR && unary_typ->tag != TYP_ARRAY)
                panic("type check error: expected pointer");
            node->type = unary_typ->base;
            break;
        }
        case NT_ASSIGN:
        case NT_ASSIGN_ADD:
        case NT_ASSIGN_SUB:
        case NT_ASSIGN_MUL:
        case NT_ASSIGN_DIV: {
            Type *lhs_typ = typed(node->expr.lhs, env)->type;
            Type *rhs_typ = typed(node->expr.rhs, env)->type;
            if (rhs_typ->tag == TYP_ARRAY) rhs_typ = pointer_to(rhs_typ->base);
            if (!is_compatible(lhs_typ, rhs_typ)) panic("type check error: incompatible type");
            node->type = rhs_typ;
            break;
        }
        case NT_FNCALL: {
            for (int i = 0; i < node->fncall.args->len; i++)
                typed(node->fncall.args->nodes[i], env);
            Symbol *func = find_symbol(ST_FUNC, env->func_types, node->main_token);
            if (!func) node->type = type_int;
            else node->type = func->type;
            break;
        }
        case NT_BLOCK:
            for (int i = 0; i < node->block->len; i++)
                typed(node->block->nodes[i], env);
            node->type = NULL;
            break;
        case NT_RETURN:
            typed(node->unary_expr, env);
            node->type = NULL;
            break;
        case NT_IF:
            typed(node->ifstmt.cond, env);
            typed(node->ifstmt.then, env);
            typed(node->ifstmt.els, env);
            node->type = NULL;
            break;
        case NT_WHILE:
            typed(node->whilestmt.cond, env);
            typed(node->whilestmt.body, env);
            node->type = NULL;
            break;
        case NT_FOR:
            typed(node->forstmt.def, env);
            typed(node->forstmt.cond, env);
            typed(node->forstmt.next, env);
            typed(node->forstmt.body, env);
            node->type = NULL;
            break;
        case NT_FUNC:
            for (int i = 0; i < node->func.params->len; i++)
                typed(node->func.params->nodes[i], env);
            typed(node->func.body, env);
            break;
        case NT_PARAMDECL:
            node->type = NULL;
            break;
        case NT_LVARDECL:
            for (int i = 0; i < node->declarators->len; i++)
                typed(node->declarators->nodes[i], env);
            node->type = NULL;
            break;
        case NT_GLOBALDECL:
            node->type = NULL;
            break;
        case NT_SIZEOF:
            typed(node->unary_expr, env);
            node->type = type_int;
            break;
        case NT_TYPE:
            panic("typed: unreachable");
            break;
    }
    return node;
}

void type_funcs(Program *prog) {
    NodeList *funcs = prog->funcs;
    Env *env = env_new(NULL, prog->global_vars, prog->func_types);
    for (int i = 0; i < funcs->len; i++) {
        Node *fnode = funcs->nodes[i];
        env->local_vars = fnode->func.locals; // set local variables
        typed(fnode, env);
    }
    free(env);
    return;
}
