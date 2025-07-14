#include "kcc.h"

Type *type_int = &(Type){TYP_INT, NULL};

Type *pointer_to(Type *base) {
    Type *ptr = calloc(1, sizeof(Type));
    ptr->tag = TYP_PTR;
    ptr->base = base;
    return ptr;
}

int sizeof_type(Type *type) {
    switch (type->tag) {
        case TYP_INT: return 4;
        case TYP_PTR: return 8;
        default: panic("node_sizeof: error");
    }
    return 0;
}

bool is_compatible(Type *a, Type *b) {
    if (a->tag == TYP_PTR && a->tag == TYP_PTR)
        return is_compatible(a->base, b->base);
    else return a->tag == b->tag;
}

Node *typed(Node *node, Var *env) {
    if (!node || node->type != NULL) return node;
    switch (node->tag) {
        case NT_INT:
            node->type = type_int;
            break;
        case NT_IDENT:
            node->type = find_local_var(env, node->main_token)->type;
            break;
        case NT_ADD:
        case NT_SUB: {
            Type *lhs_typ = typed(node->expr.lhs, env)->type;
            Type *rhs_typ = typed(node->expr.rhs, env)->type;
            if (lhs_typ->tag == TYP_INT && rhs_typ->tag == TYP_INT) node->type = type_int;
            else if (lhs_typ->tag == TYP_PTR && rhs_typ->tag == TYP_INT) node->type = lhs_typ;
            else if (lhs_typ->tag == TYP_INT && rhs_typ->tag == TYP_PTR) node->type = rhs_typ;
            else panic("undefined: ptr + ptr / unimplemented: ptr - ptr");
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
            node->type = typed(node->unary_expr, env)->type;
            break;
        case NT_ADDR: {
            Type *base = typed(node->unary_expr, env)->type;
            node->type = pointer_to(base);
            break;
        }
        case NT_DEREF: {
            Type *unary_typ = typed(node->unary_expr, env)->type;
            if (unary_typ->tag != TYP_PTR) panic("type check error: expected pointer");
            node->type = unary_typ->base;
            break;
        }
        case NT_ASSIGN: {
            Type *lhs_typ = typed(node->expr.lhs, env)->type;
            Type *rhs_typ = typed(node->expr.rhs, env)->type;
            if (!is_compatible(lhs_typ, rhs_typ)) panic("type check error: incompatible type");
            node->type = rhs_typ;
            break;
        }
        case NT_ASSIGN_ADD:
        case NT_ASSIGN_SUB:
        case NT_ASSIGN_MUL:
        case NT_ASSIGN_DIV: {
            typed(node->expr.lhs, env);
            node->type = typed(node->expr.rhs, env)->type;
            break;
        }
        case NT_FNCALL:
            for (int i = 0; i < node->fncall.args->len; i++)
                typed(node->fncall.args->nodes[i], env);
            node->type = type_int; // todo: determining by function return type
            break;
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
            node->type = type_int; // TODO
            break;
        case NT_VARDECL:
            node->type = NULL;
            break;
        case NT_SIZEOF:
            typed(node->unary_expr, env);
            node->type = type_int;
            break;
    }
    return node;
}
