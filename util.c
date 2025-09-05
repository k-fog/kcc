#include "kcc.h"

Stack* stack_new(int capacity) {
    Stack *stack = calloc(1, sizeof(Stack));
    stack->capacity = capacity;
    stack->top = 0;
    stack->data = calloc(capacity, sizeof(int));
    return stack;
}

int stack_top(Stack *stack) {
    if (stack->top == 0) panic("internal error: stack underflow");
    return stack->data[stack->top - 1];
}

int stack_pop(Stack *stack) {
    if (stack->top == 0) panic("internal error: stack underflow");
    return stack->data[--stack->top];
}

void stack_push(Stack *stack, int val) {
    if (stack->capacity <= stack->top) {
        stack->capacity = stack->capacity * 2 + 1;
        int *tmp = realloc(stack->data, stack->capacity * sizeof(int));
        if (!tmp) panic("internal error: realloc failed");
        stack->data = tmp;
    }
    stack->data[stack->top++] = val;
}
