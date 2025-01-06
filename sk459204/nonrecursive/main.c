#include <stddef.h>

#include "common/io.h"
#include "common/sumset.h"

#include <stdbool.h>
#include <stdlib.h>

typedef struct PairToSolve {
    Sumset* a;
    Sumset* b;
} PairToSolve_t;

typedef struct Stack {
    PairToSolve_t** stack;
    int last_push_index;
    int stack_size;
} Stack_t;

Stack_t* stack_init(int stack_size) {
    Stack_t* stack = (Stack_t*) malloc(sizeof(Stack_t));
    stack->stack = (PairToSolve_t**) malloc(stack_size * sizeof(PairToSolve_t*));
    stack->last_push_index = -1;
    stack->stack_size = stack_size;
    return stack;
}

void stack_push(Stack_t* stack, PairToSolve_t* pair) {
    stack->stack[++stack->last_push_index] = pair;
}

PairToSolve_t* stack_pop(Stack_t* stack) {
    PairToSolve_t* result = stack->stack[stack->last_push_index];
    stack->stack[stack->last_push_index--] = NULL;

    return result;
}

bool stack_is_empty(Stack_t* stack) {
    return stack->last_push_index == -1;
}

void stack_destroy(Stack_t* stack) {
    free(stack->stack);
    free(stack);
}

void sumset_swap(Sumset** a, Sumset** b) {
    Sumset* tmp = *a;
    *a = *b;
    *b = tmp;
}

void nonrecursive_complete_dummy_solv(InputData* input_data, Solution* best_solution) 
{      
    Stack_t* stack = stack_init(8192);

    PairToSolve_t* pair = (PairToSolve_t*) malloc(sizeof(PairToSolve_t));
    pair->a = &input_data->a_start; 
    pair->b = &input_data->b_start;

    stack_push(stack, pair);

    while (!stack_is_empty(stack)) {
        PairToSolve_t* pair = stack_pop(stack);
        Sumset* a = pair->a;
        Sumset* b = pair->b;
        free(pair);

        if (a->sum > b->sum) {
            sumset_swap(&a, &b);
        }

        if (is_sumset_intersection_trivial(a, b)) { // s(a) ∩ s(b) = {0}.
            for (size_t i = a->last; i <= input_data->d; ++i) {
                if (!does_sumset_contain(b, i)) {
                    Sumset* a_with_i = (Sumset*) malloc(sizeof(Sumset));
                    sumset_add(a_with_i, a, i);

                    PairToSolve_t* new_pair = (PairToSolve_t*) malloc(sizeof(PairToSolve_t));
                    new_pair->a = a_with_i;
                    new_pair->b = b;

                    stack_push(stack, new_pair);
                }
            }
        } else if ((a->sum == b->sum) && (get_sumset_intersection_size(a, b) == 2)) { // s(a) ∩ s(b) = {0, ∑b}.
            if (a->sum > best_solution->sum) {
                solution_build(best_solution, input_data, a, b);
            }
        }
    }
}

int main()
{
    InputData input_data;
    //input_data_read(&input_data);
    input_data_init(&input_data, 8, 24, (int[]){0}, (int[]){1, 0});

    Solution best_solution;
    solution_init(&best_solution);

    nonrecursive_complete_dummy_solv(&input_data, &best_solution);

    solution_print(&best_solution);
    return 0;
}
