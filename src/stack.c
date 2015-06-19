#include <stdio.h>
#include <stdlib.h>
#include "zmalloc.h"
#include "stack.h"

void stack_init(stack_t *stack, int max_size) {
	stack->contents = zmalloc(sizeof(void *) * max_size);;
	stack->max_size = max_size;
	stack->top = -1;
	stack->rtop = 0;
}

void stack_destroy(stack_t *stack) {
	/* Get rid of array. */
	zfree(stack->contents);

	stack->contents = NULL;
	stack->max_size = 0;
	stack->top = -1;
	stack->rtop = 0;
}

void stack_push(stack_t *stack, void *element) {
	if (stack_isfull(stack))
		stack->contents = zrealloc(stack->contents, sizeof(void *) * (stack->max_size*=2));

	/* Put information in array; update top. */
	stack->contents[++stack->top] = element;
}

void *stack_pop(stack_t *stack) {
	if (stack_isempty(stack))
		return NULL;

	return stack->contents[stack->top--];
}

void *stack_peek(stack_t *stack) {
	if (stack_isempty(stack))
		return NULL;

	return stack->contents[stack->top];
}

void *stack_rpeek(stack_t *stack) {
	if (stack_isempty(stack))
		return NULL;

	return stack->contents[0];
}

void *stack_rpop(stack_t *stack) {
	if (stack_isempty(stack))
		return NULL;

	return stack->contents[stack->rtop++];
}

int stack_isempty(stack_t *stack) {
	return stack->top < 0;
}

int stack_isfull(stack_t *stack) {
	return stack->top >= stack->max_size - 1;
}
