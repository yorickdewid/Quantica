#ifndef STACK_H_INCLUDED
#define STACK_H_INCLUDED

typedef struct {
	void **contents;
	int max_size;
	int size;
	int top;
	int rtop;
} stack_t;

void stack_init(stack_t *stack, int max_size);
void stack_destroy(stack_t *stack);
void stack_push(stack_t *stack, void *elm);
void *stack_pop(stack_t *stack);
void *stack_rpop(stack_t *stack);
void *stack_offset_peek(stack_t *stack, int offset);
void *stack_offset_rpeek(stack_t *stack, int offset);
void *stack_peek(stack_t *stack);
void *stack_rpeek(stack_t *stack);

#endif // STACK_H_INCLUDED