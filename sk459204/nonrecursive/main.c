#include <stddef.h>

#include "common/io.h"
#include "common/sumset.h"

#include <stdbool.h>
#include <stdlib.h>

typedef struct SmartSumset {
    Sumset sumset;
    struct SmartSumset* parent;
    int reference_count;
} SmartSumset_t;

typedef struct PairToSolve {
    SmartSumset_t* a;
    SmartSumset_t* b;
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

void smart_sumset_swap(SmartSumset_t** a, SmartSumset_t** b) {
    SmartSumset_t* tmp = *a;
    *a = *b;
    *b = tmp;
}

void check_sumset_reference_count(SmartSumset_t* sumset) {
    while (sumset != NULL && sumset->reference_count == 0) {
        SmartSumset_t* tmp = sumset;
        sumset = sumset->parent;
        free(tmp);

        if (sumset != NULL) {
            sumset->reference_count--;
        }
    }
}

void nonrecursive_dummy_solv_no_leaks(InputData* input_data, Solution* best_solution) {
    Stack_t* stack = stack_init(8192);

    SmartSumset_t* a = (SmartSumset_t*) malloc(sizeof(SmartSumset_t));
    a->sumset = input_data->a_start;
    a->parent = NULL;
    a->reference_count = 2;

    SmartSumset_t* b = (SmartSumset_t*) malloc(sizeof(SmartSumset_t));
    b->sumset = input_data->b_start;
    b->parent = NULL;
    b->reference_count = 2;

    PairToSolve_t* pairToPush = (PairToSolve_t*) malloc(sizeof(PairToSolve_t));
    pairToPush->a = a; 
    pairToPush->b = b;

    stack_push(stack, pairToPush);

    while (!stack_is_empty(stack)) {
        PairToSolve_t* pair = stack_pop(stack);
        SmartSumset_t* a = pair->a; 
        SmartSumset_t* b = pair->b;

        if (a->sumset.sum > b->sumset.sum) {
            smart_sumset_swap(&a, &b);
        }

        if (is_sumset_intersection_trivial(&a->sumset, &b->sumset)) { // s(a) ∩ s(b) = {0}.
            for (size_t i = a->sumset.last; i <= input_data->d; ++i) {
                if (!does_sumset_contain(&b->sumset, i)) {
                    SmartSumset_t* a_with_i = (SmartSumset_t*) malloc(sizeof(SmartSumset_t));
                    a_with_i->reference_count = 1;
                    a_with_i->parent = a;
                    sumset_add(&a_with_i->sumset, &a->sumset, i);

                    PairToSolve_t* new_pair = (PairToSolve_t*) malloc(sizeof(PairToSolve_t));
                    new_pair->a = a_with_i;
                    new_pair->b = b;

                    a->reference_count++;
                    b->reference_count++;

                    stack_push(stack, new_pair);
                }
            }
        } else if ((a->sumset.sum == b->sumset.sum) && (get_sumset_intersection_size(&a->sumset, &b->sumset) == 2)) { // s(a) ∩ s(b) = {0, ∑b}.
            if (a->sumset.sum > best_solution->sum) {
                solution_build(best_solution, input_data, &a->sumset, &b->sumset);
            }
        }

        a->reference_count--;
        b->reference_count--;
        
        check_sumset_reference_count(a);
        check_sumset_reference_count(b);

        free(pair);
    }

    stack_destroy(stack);
    free(a);
    free(b);
}

int main()
{
    InputData input_data;
    //input_data_read(&input_data);
    input_data_init(&input_data, 8, 10, (int[]){0}, (int[]){1, 0});

    Solution best_solution;
    solution_init(&best_solution);

    nonrecursive_dummy_solv_no_leaks(&input_data, &best_solution);

    solution_print(&best_solution);
    return 0;
}
