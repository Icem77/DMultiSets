#include <stddef.h>

#include "common/io.h"
#include "common/sumset.h"

#include <stdbool.h>
#include <stdlib.h>

typedef struct SmartSumset {
    Sumset sumset;
    struct SmartSumset* parent;
    int reference_count;

    struct SmartSumset* next_on_free_list; // connection in memory pool
} SmartSumset_t;

typedef struct Stack {
    SmartSumset_t** stack;
    int last_push_index;
    int stack_size;
} Stack_t;

typedef struct SmartSumsetPool {
    SmartSumset_t* pool;
    SmartSumset_t* free_list;
    int pool_size;
} SmartSumsetPool_t;

SmartSumsetPool_t* pool_init(int pool_size) {
    SmartSumsetPool_t* pool = (SmartSumsetPool_t*) malloc(sizeof(SmartSumsetPool_t));
    pool->pool = (SmartSumset_t*) malloc(pool_size * sizeof(SmartSumset_t));
    pool->free_list = &pool->pool[0];
    pool->pool_size = pool_size;

    for (int i = 0; i < pool_size - 1; ++i) {
        pool->pool[i].next_on_free_list = &pool->pool[i + 1];
    }
    pool->pool[pool_size - 1].next_on_free_list = NULL;

    return pool;
}

SmartSumset_t* pool_get(SmartSumsetPool_t* pool) {
    if (pool->free_list == NULL) {
        pool->pool = (SmartSumset_t*) realloc(pool->pool, 2 * pool->pool_size * sizeof(SmartSumset_t));
        pool->pool_size *= 2;
        pool->free_list = &pool->pool[pool->pool_size / 2];

        for (int i = pool->pool_size / 2; i < pool->pool_size - 1; ++i) {
            pool->pool[i].next_on_free_list = &pool->pool[i + 1];
        }
        pool->pool[pool->pool_size - 1].next_on_free_list = NULL;
    }

    SmartSumset_t* result = pool->free_list;
    pool->free_list = result->next_on_free_list;

    return result;
}

void pool_return(SmartSumsetPool_t* pool, SmartSumset_t* smart_sumset) {
    smart_sumset->next_on_free_list = pool->free_list;
    pool->free_list = smart_sumset;
}

void pool_destroy(SmartSumsetPool_t* pool) {
    free(pool->pool);
    free(pool);
}

Stack_t* stack_init(int stack_size) {
    Stack_t* stack = (Stack_t*) malloc(sizeof(Stack_t));
    stack->stack = (SmartSumset_t**) malloc(stack_size * sizeof(SmartSumset_t*));
    stack->last_push_index = -1;
    stack->stack_size = stack_size;
    return stack;
}

void stack_push(Stack_t* stack, SmartSumset_t* a, SmartSumset_t* b) {
    stack->last_push_index += 2;
    if (stack->last_push_index == stack->stack_size) {
        stack->stack = (SmartSumset_t**) realloc(stack->stack, 2 * stack->stack_size * sizeof(SmartSumset_t*));
        stack->stack_size *= 2;
    }

    stack->stack[stack->last_push_index - 1] = a;
    stack->stack[stack->last_push_index] = b;
}

void stack_pop(Stack_t* stack, SmartSumset_t** a, SmartSumset_t** b) {
    *b = stack->stack[stack->last_push_index];
    *a = stack->stack[stack->last_push_index - 1];
    stack->last_push_index -= 2;
}

bool stack_is_empty(Stack_t* stack) {
    return stack->last_push_index == -1;
}

void stack_destroy(Stack_t* stack) {
    free(stack->stack);
    free(stack);
}

void smart_sumset_swap(SmartSumset_t** a, SmartSumset_t** b) {
    SmartSumset_t* tmp = *a;
    *a = *b;
    *b = tmp;
}

void check_sumset_reference_count(SmartSumsetPool_t* pool, SmartSumset_t* sumset) {
    while (sumset != NULL && --sumset->reference_count == 0) {
        SmartSumset_t* tmp = sumset;
        sumset = sumset->parent;
        pool_return(pool, tmp);
    }
}

void nonrecursive_pool_solv_no_pairs(InputData* input_data, Solution* best_solution) {
    SmartSumsetPool_t* pool = pool_init(1024);

    SmartSumset_t* a = pool_get(pool);
    a->sumset = input_data->a_start;
    a->parent = NULL;
    a->reference_count = 2;

    SmartSumset_t* b = pool_get(pool);
    b->sumset = input_data->b_start;
    b->parent = NULL;
    b->reference_count = 2;

    Stack_t* stack = stack_init(4096);
    stack_push(stack, a, b);

    int counter;

    while (!stack_is_empty(stack)) {
        stack_pop(stack, &a, &b);

        if (a->sumset.sum > b->sumset.sum) {
            smart_sumset_swap(&a, &b);
        }

        if (is_sumset_intersection_trivial(&a->sumset, &b->sumset)) { // s(a) ∩ s(b) = {0}.
            counter = 0;
            for (size_t i = a->sumset.last; i <= input_data->d; ++i) {
                if (!does_sumset_contain(&b->sumset, i)) {
                    SmartSumset_t* a_with_i = pool_get(pool);
                    a_with_i->reference_count = 1;
                    a_with_i->parent = a;
                    sumset_add(&a_with_i->sumset, &a->sumset, i);

                    counter++;

                    stack_push(stack, a_with_i, b);
                }
            }

            a->reference_count += counter;
            b->reference_count += counter;
        } else if ((a->sumset.sum == b->sumset.sum) && (get_sumset_intersection_size(&a->sumset, &b->sumset) == 2)) { // s(a) ∩ s(b) = {0, ∑b}.
            if (a->sumset.sum > best_solution->sum) {
                solution_build(best_solution, input_data, &a->sumset, &b->sumset);
            }
        }
        check_sumset_reference_count(pool, a);
        check_sumset_reference_count(pool, b);
    }

    stack_destroy(stack);
    pool_destroy(pool);
}

int main()
{
    InputData input_data;
    //input_data_read(&input_data);
    input_data_init(&input_data, 8, 34, (int[]){0}, (int[]){1, 0});

    Solution best_solution;
    solution_init(&best_solution);

    nonrecursive_pool_solv_no_pairs(&input_data, &best_solution);

    solution_print(&best_solution);
    return 0;
}
