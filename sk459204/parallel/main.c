#include <stddef.h>

#include "common/io.h"
#include "common/sumset.h"

#include <pthread.h>
#include <stdatomic.h>

#include <stdlib.h>
#include <stdio.h>

#define INITIAL_POOL_SIZE 8192

typedef struct SmartParallelSumset {
    Sumset sumset;
    struct SmartParallelSumset* parent;
    atomic_int reference_count;

    struct SmartParallelSumset* next_on_free_list; // connection in memory pool
} SmartParallelSumset_t;

typedef struct SmartParallelSumsetPool {
    SmartParallelSumset_t* pool;
    SmartParallelSumset_t* free_list;
    int pool_size;

    pthread_mutex_t mutex;
} SmartParallelSumsetPool_t;

typedef struct SharedStack {
    SmartParallelSumset_t** stack;
    atomic_int last_push_index; // do not use atomic cause we have mutex
    int stack_size;

    pthread_mutex_t mutex;
    pthread_cond_t waiting_room;
    int threads_waiting; // to know when to finish
    bool calculations_finished;
} SharedStack_t;

typedef struct ThreadStarterPack {
    SharedStack_t* common_stack;
    SmartParallelSumsetPool_t* common_pool;
    InputData* input_data;
    Solution* my_best_solution;
} ThreadStarterPack_t;

SharedStack_t* shared_stack_init(int stack_size) {
    SharedStack_t* stack = (SharedStack_t*) malloc(sizeof(SharedStack_t));
    stack->stack = (SmartParallelSumset_t**) malloc(stack_size * sizeof(SmartParallelSumset_t*));
    atomic_store(&stack->last_push_index, -1);
    stack->stack_size = stack_size;
    stack->threads_waiting = 0;
    stack->calculations_finished = false;

    pthread_mutex_init(&stack->mutex, NULL); // general mutex for stack
    pthread_cond_init(&stack->waiting_room, NULL); // place to wait if nothing to pop

    return stack;
}

void shared_stack_destroy(SharedStack_t* stack) {
    free(stack->stack);
    pthread_mutex_destroy(&stack->mutex);
    free(stack);
}

void give_away_branches(SharedStack_t* stack, SmartParallelSumset_t** sumsets, int counter) {
    pthread_mutex_lock(&stack->mutex);
    for (int i = 1; i <= counter; ++i) {
        stack->stack[stack->last_push_index + i] = sumsets[i];
    }
    atomic_fetch_add(&stack->last_push_index, counter);
    pthread_mutex_unlock(&stack->mutex);
    pthread_cond_signal(&stack->waiting_room);
}

void take_new_branch(SharedStack_t* stack, SmartParallelSumset_t** a, SmartParallelSumset_t** b, int workingThreads) {
    pthread_mutex_lock(&stack->mutex);
    if (stack->last_push_index == -1 && stack->threads_waiting == workingThreads - 1) { // koniec obliczen (skonczylismy jako ostatni)
        stack->calculations_finished = true;
        pthread_mutex_unlock(&stack->mutex);
        pthread_cond_broadcast(&stack->waiting_room);
        *b = NULL;
        *a = NULL;
        return;
    } else if (stack->last_push_index == -1) {
        while (stack->last_push_index == -1 && !stack->calculations_finished) {
            stack->threads_waiting++;
            pthread_cond_wait(&stack->waiting_room, &stack->mutex);
            stack->threads_waiting--;
        }
    }

    if (stack->calculations_finished) {
        pthread_mutex_unlock(&stack->mutex);
        *b = NULL;
        *a = NULL;
        return; // zostalismy wybudzeni do skonczenia pracy
    }

    *b = stack->stack[stack->last_push_index];
    *a = stack->stack[stack->last_push_index - 1];
    atomic_fetch_sub(&stack->last_push_index, 2);

    pthread_mutex_unlock(&stack->mutex);
}

SmartParallelSumsetPool_t* pool_init(int pool_size) {
    SmartParallelSumsetPool_t* pool = (SmartParallelSumsetPool_t*) malloc(sizeof(SmartParallelSumsetPool_t));
    pool->pool = (SmartParallelSumset_t*) malloc(pool_size * sizeof(SmartParallelSumset_t));
    pool->free_list = &pool->pool[0];
    pool->pool_size = pool_size;

    for (int i = 0; i < pool_size - 1; ++i) {
        pool->pool[i].next_on_free_list = &pool->pool[i + 1];
    }
    pool->pool[pool_size - 1].next_on_free_list = NULL;

    pthread_mutex_init(&pool->mutex, NULL);

    return pool;
}

void pool_destroy(SmartParallelSumsetPool_t* pool) {
    free(pool->pool);
    pthread_mutex_destroy(&pool->mutex);
    free(pool);
}

SmartParallelSumset_t* pool_get(SmartParallelSumsetPool_t* pool, int how_much) {
    pthread_mutex_lock(&pool->mutex);
    if (pool->free_list == NULL) {
        pool->pool = (SmartParallelSumset_t*) realloc(pool->pool, 2 * pool->pool_size * sizeof(SmartParallelSumset_t));
        pool->pool_size *= 2;
        pool->free_list = &pool->pool[pool->pool_size / 2];

        for (int i = pool->pool_size / 2; i < pool->pool_size - 1; ++i) {
            pool->pool[i].next_on_free_list = &pool->pool[i + 1];
        }
        pool->pool[pool->pool_size - 1].next_on_free_list = NULL;
    }

    int i = 1;
    SmartParallelSumset_t* result = pool->free_list;
    SmartParallelSumset_t* tail = result;
    while (i < how_much) {
        tail = tail->next_on_free_list;
        i++;
    }
    pool->free_list = tail->next_on_free_list;
    tail->next_on_free_list = NULL;
    pthread_mutex_unlock(&pool->mutex);

    return result;
}

void pool_return(SmartParallelSumsetPool_t* pool, SmartParallelSumset_t* returning) {
    pthread_mutex_lock(&pool->mutex);
    returning->next_on_free_list = pool->free_list;
    pool->free_list = returning;
    pthread_mutex_unlock(&pool->mutex);
}

void swap(SmartParallelSumset_t** a, SmartParallelSumset_t** b) {
    SmartParallelSumset_t* tmp = *a;
    *a = *b;
    *b = tmp;
}

void branch_break(ThreadStarterPack_t* resources, SmartParallelSumset_t* a, SmartParallelSumset_t* b) {
    if (a->sumset.sum > b->sumset.sum) swap(&a, &b);

    if (is_sumset_intersection_trivial(&a->sumset, &b->sumset)) { // s(a) ∩ s(b) = {0}.
        int counter = 0;
        int expansions[resources->input_data->d - a->sumset.last + 1];
        for (size_t i = a->sumset.last; i <= resources->input_data->d; ++i) {
            if (!does_sumset_contain(&b->sumset, i)) {
                expansions[counter] = i;
                counter++;
            }
        }

        SmartParallelSumset_t* sumset = pool_get(resources->common_pool, counter);
        SmartParallelSumset_t* expandedSumsets[2*counter];
        for (int i = 0; i < counter; i += 2) {
            sumset_add(&sumset->sumset, &a->sumset, expansions[i]);
            atomic_store(&sumset->reference_count, 0);
            expandedSumsets[i] = sumset;
            expandedSumsets[i + 1] = b;
            sumset = sumset->next_on_free_list;
        }

        atomic_fetch_add(&a->reference_count, counter);
        atomic_fetch_add(&b->reference_count, counter);
        give_away_branches(resources->common_stack, expandedSumsets, 2*counter);

    } else if ((a->sumset.sum == b->sumset.sum) && (get_sumset_intersection_size(&a->sumset, &b->sumset) == 2)) { // s(a) ∩ s(b) = {0, ∑b}.
        if (a->sumset.sum > resources->my_best_solution->sum) {
            solution_build(resources->my_best_solution, resources->input_data, &a->sumset, &b->sumset);
        }
    }
}

void recursive_branch_solve(ThreadStarterPack_t* resources, SmartParallelSumset_t* a, SmartParallelSumset_t* b) {
    if (a->sumset.sum > b->sumset.sum) recursive_branch_solve(resources, b, a);

    if (is_sumset_intersection_trivial(&a->sumset, &b->sumset)) { // s(a) ∩ s(b) = {0}.
        for (size_t i = a->sumset.last; i <= resources->input_data->d; ++i) {
            if (!does_sumset_contain(&b->sumset, i)) {
                SmartParallelSumset_t a_with_i;
                sumset_add(&a_with_i.sumset, &a->sumset, i);
                a_with_i.parent = a;
                atomic_store(&a_with_i.reference_count, 0);
                recursive_branch_solve(resources, &a_with_i, b);
            }
        }
    } else if ((a->sumset.sum == b->sumset.sum) && (get_sumset_intersection_size(&a->sumset, &b->sumset) == 2)) { // s(a) ∩ s(b) = {0, ∑b}.
        if (b->sumset.sum > resources->my_best_solution->sum)
            solution_build(resources->my_best_solution, resources->input_data, &a->sumset, &b->sumset);
    }
}

void clean_up(SmartParallelSumsetPool_t* pool, SmartParallelSumset_t* check) {
    if (atomic_fetch_sub(&check->reference_count, 1) == 1) {
        SmartParallelSumset_t* toCheck = check->parent;
        pool_return(pool, check);
        clean_up(pool, toCheck);
    }
}

void* thread_calculations(void* args) {
    ThreadStarterPack_t* resources = (ThreadStarterPack_t*) args;
    SmartParallelSumset_t* a;
    SmartParallelSumset_t* b;

    take_new_branch(resources->common_stack, &a, &b, resources->input_data->t);
    
    while (a != NULL && b != NULL) {
        if (atomic_load(&resources->common_stack->last_push_index) + 1 < 2 * resources->input_data->t) {
            branch_break(resources, a, b);
        } else {
            recursive_branch_solve(resources, a, b);
            if (a->parent != NULL) clean_up(resources->common_pool, a->parent);
            if (b->parent != NULL) clean_up(resources->common_pool, b->parent);
        }

        take_new_branch(resources->common_stack, &a, &b, resources->input_data->t);
    }

    return NULL;
}

int main()
{   
    /*
    ZAŁOŻENIE REKURENCYJNEJ WERSJI PROGRAMU:
    - wez branch ze wspolnej puli pracy
    - -> jezeli na stosie jest wystarczajacy zapas zlecen:
            - rekurencyjnie rozwiaz branch
            - sprawdz czy nalezy zwolnic fundament na ktorym byl oparty branch
            - podczas rekurencji zbierz liste Sumsetow do uwolnienia i oddaj ja do puli
    - -> wpp:
            - wez z puli sumsety i rozbij branch 
            - oddaj rozbite branche na stos
    */

    InputData input_data;
    //input_data_read(&input_data);
    input_data_init(&input_data, 1, 4, (int[]){0}, (int[]){1, 0});

    SharedStack_t* common_stack = shared_stack_init(5000);
    SmartParallelSumsetPool_t* common_pool = pool_init(input_data.t * INITIAL_POOL_SIZE);

    SmartParallelSumset_t* a = pool_get(common_pool, 1); //pool_get(common_pool);
    a->sumset = input_data.a_start;
    a->parent = NULL;
    atomic_store(&a->reference_count, 10);

    SmartParallelSumset_t* b = pool_get(common_pool, 1);
    b->sumset = input_data.b_start;
    b->parent = NULL;
    atomic_store(&b->reference_count, 10);

    SmartParallelSumset_t* start[2];
    start[0] = a;
    start[1] = b;
    give_away_branches(common_stack, start, 2);
    
    pthread_t threads[input_data.t];
    ThreadStarterPack_t* starter_packs = (ThreadStarterPack_t*) malloc(input_data.t * sizeof(ThreadStarterPack_t)); 
    Solution* best_solutions = (Solution*) malloc(input_data.t * sizeof(Solution));
    for (int i = 0; i < input_data.t; i++) {
        solution_init(&best_solutions[i]);
        starter_packs[i].my_best_solution = &best_solutions[i];
        starter_packs[i].common_stack = common_stack;
        starter_packs[i].common_pool = common_pool;
        starter_packs[i].input_data = &input_data;
        pthread_create(&threads[i], NULL, thread_calculations, (void*)&starter_packs[i]);
    }

    // wait for the end of calculations
    for (int i = 0; i < input_data.t; ++i) {
        pthread_join(threads[i], NULL);
    }

    // find the best solution
    Solution* best_solution;
    int best_sum = 0;
    for (int i = 0; i < input_data.t; i++) {
        if (best_solutions[i].sum >= best_sum) {
            best_sum = best_solutions[i].sum;
            best_solution = &best_solutions[i];
        }
    }

    solution_print(best_solution);

    free(best_solutions);
    free(starter_packs);
    shared_stack_destroy(common_stack);
    pool_destroy(common_pool);

    return 0;
}
