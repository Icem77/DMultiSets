#include <stddef.h>

#include "common/io.h"
#include "common/sumset.h"

#include <pthread.h>
#include <stdatomic.h>

#include <stdlib.h>
#include <stdio.h>

#define ITERATIONS_TO_GIVEAWAY 10
#define INITIAL_POOL_SIZE 1024
#define POOL_POP_PACKACGE_SIZE 64
#define STACK_POP_PACKAGE_SIZE 64

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
    int free_list_size;

    pthread_mutex_t mutex;
} SmartParallelSumsetPool_t;

typedef struct SharedStack {
    SmartParallelSumset_t** stack;
    int last_push_index; // do not use atomic cause we have mutex
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

SharedStack_t* stack_init(int stack_size) {
    SharedStack_t* stack = (SharedStack_t*) malloc(sizeof(SharedStack_t));
    stack->stack = (SmartParallelSumset_t**) malloc(stack_size * sizeof(SmartParallelSumset_t*));
    stack->last_push_index = -1;
    stack->stack_size = stack_size;
    stack->threads_waiting = 0;
    stack->calculations_finished = false;

    pthread_mutex_init(&stack->mutex, NULL); // general mutex for stack
    pthread_cond_init(&stack->waiting_room, NULL); // place to wait if nothing to pop

    return stack;
}

void stack_destroy(SharedStack_t* stack) {
    free(stack->stack);
    pthread_mutex_destroy(&stack->mutex);
    free(stack);
}

void give_away_branch(SharedStack_t* stack, SmartParallelSumset_t* a, SmartParallelSumset_t* b) {
    pthread_mutex_lock(&stack->mutex);
    stack->last_push_index += 2;
    stack->stack[stack->last_push_index - 1] = a;
    stack->stack[stack->last_push_index] = b;
    pthread_mutex_unlock(&stack->mutex);
    pthread_cond_signal(&stack->waiting_room);
}

void take_new_branch(SharedStack_t* stack, SmartParallelSumset_t** a, SmartParallelSumset_t** b, int workingThreads) {
    pthread_mutex_lock(&stack->mutex);
    if (stack->last_push_index == -1 && stack->threads_waiting == workingThreads - 1) { // koniec obliczen (skonczylismy jako ostatni)
        printf("ZAKONCZENIE OBLICZEN\n");
        stack->calculations_finished = true;
        pthread_mutex_unlock(&stack->mutex);
        pthread_cond_broadcast(&stack->waiting_room);
        *b = NULL;
        *a = NULL;
        return;
    } else if (stack->last_push_index == -1) {
        while (stack->last_push_index == -1 && !stack->calculations_finished) {
            printf("GOING TO SLEEP\n");
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
    stack->last_push_index -= 2;

    pthread_mutex_unlock(&stack->mutex);
}

SmartParallelSumsetPool_t* pool_init(int pool_size) {
    SmartParallelSumsetPool_t* pool = (SmartParallelSumsetPool_t*) malloc(sizeof(SmartParallelSumsetPool_t));
    pool->pool = (SmartParallelSumset_t*) malloc(pool_size * sizeof(SmartParallelSumset_t));
    pool->free_list = &pool->pool[0];
    pool->pool_size = pool_size;
    pool->free_list_size = pool_size;

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

SmartParallelSumset_t* pool_get(SmartParallelSumsetPool_t* pool) {
    pthread_mutex_lock(&pool->mutex);
    if (pool->free_list == NULL) {
        pool->pool = (SmartParallelSumset_t*) realloc(pool->pool, 2 * pool->pool_size * sizeof(SmartParallelSumset_t));
        pool->pool_size *= 2;
        pool->free_list = &pool->pool[pool->pool_size / 2];
        pool->free_list_size = pool->pool_size / 2;

        for (int i = pool->pool_size / 2; i < pool->pool_size - 1; ++i) {
            pool->pool[i].next_on_free_list = &pool->pool[i + 1];
        }
        pool->pool[pool->pool_size - 1].next_on_free_list = NULL;
    }

    SmartParallelSumset_t* result = pool->free_list;
    pool->free_list = result->next_on_free_list;
    pool->free_list_size -= 1;
    pthread_mutex_unlock(&pool->mutex);

    return result;
}

void pool_return(SmartParallelSumsetPool_t* pool, SmartParallelSumset_t* smart_sumset) {
    pthread_mutex_lock(&pool->mutex);
    pool->free_list_size += 1;
    smart_sumset->next_on_free_list = pool->free_list;
    pool->free_list = smart_sumset;
    pthread_mutex_unlock(&pool->mutex);
}

int free_list_size(SmartParallelSumsetPool_t* pool) {
    return pool->free_list_size;
}

void check_reference_count(SmartParallelSumsetPool_t* pool, SmartParallelSumset_t* sumset) {
    if (atomic_load(&sumset->reference_count) == 0) {
        SmartParallelSumset_t* tmp = sumset;
        sumset = sumset->parent;
        pool_return(pool, tmp);

        if (sumset != NULL) {
            atomic_fetch_sub(&sumset->reference_count, 1);
            check_reference_count(pool, sumset);    
        }
    }
}

void recursive_work(ThreadStarterPack_t* info, int *iterations_to_giveaway, SmartParallelSumset_t* a, SmartParallelSumset_t* b) {
    if (a->sumset.sum > b->sumset.sum) {
        recursive_work(info, iterations_to_giveaway, b, a);
    } else {
        if (is_sumset_intersection_trivial(&a->sumset, &b->sumset)) { 
            for (size_t i = a->sumset.last; i <= info->input_data->d; ++i) {
                if (!does_sumset_contain(&b->sumset, i)) {
                    SmartParallelSumset_t* a_with_i = pool_get(info->common_pool);
                    sumset_add(&a_with_i->sumset, &a->sumset, i);
                    atomic_store(&a_with_i->reference_count, 1);
                    a_with_i->parent = a;
                    
                    recursive_work(info, iterations_to_giveaway, a_with_i, b);
                    pool_return(info->common_pool, a_with_i);
                }
            }

        } else if ((a->sumset.sum == b->sumset.sum) && (get_sumset_intersection_size(&a->sumset, &b->sumset) == 2)) { // s(a) ∩ s(b) = {0, ∑b}.
            if (a->sumset.sum > info->my_best_solution->sum) {
                solution_build(info->my_best_solution, info->input_data, &a->sumset, &b->sumset);
            }
        }
    }
}

void* thread_calculation(void* args) {
    ThreadStarterPack_t* starter_pack = (ThreadStarterPack_t*) args;

    SmartParallelSumset_t* a = NULL;
    SmartParallelSumset_t* b = NULL;
    int till_giveaway = ITERATIONS_TO_GIVEAWAY;
    do {
        take_new_branch(starter_pack->common_stack, &a, &b, starter_pack->input_data->t);
        if (a != NULL && b != NULL) {
            recursive_work(starter_pack, &till_giveaway, a, b);
        }
    } while (a != NULL && b != NULL);

    return NULL;
}

int main()
{   
    /*
    ZAŁOŻENIE PIERWOTNEJ WERSJI PROGRAMU:
    - program wykonuje poprawnie obliczenia
    - program jest pozbawiony wyciekow pamieci
    - program jest pozbawiony bledow synchronizacyjnych
    - program wykorzystuje naiwna taktyke oddawania i pobierania zadan ze stosu
    - program wykorzystuje naiwna taktyke oddawania i pobierania sumsetow z puli
    */

    InputData input_data;
    //input_data_read(&input_data);
    input_data_init(&input_data, 12, 16, (int[]){0}, (int[]){1, 0});

    SharedStack_t* common_stack = stack_init(4096);
    SmartParallelSumsetPool_t* common_pool = pool_init(input_data.t * INITIAL_POOL_SIZE);

    printf("INITIAL POOL FREE SIZE: %d\n", free_list_size(common_pool));

    SmartParallelSumset_t* a = pool_get(common_pool);
    a->sumset = input_data.a_start;
    a->parent = NULL;
    atomic_store(&a->reference_count, 2);

    SmartParallelSumset_t* b = pool_get(common_pool);
    b->sumset = input_data.b_start;
    b->parent = NULL;
    atomic_store(&a->reference_count, 2);

    give_away_branch(common_stack, a, b);
    
    ThreadStarterPack_t* starter_packs = (ThreadStarterPack_t*) malloc(input_data.t * sizeof(ThreadStarterPack_t)); 
    Solution* best_solutions = (Solution*) malloc(input_data.t * sizeof(Solution));
    for (int i = 0; i < input_data.t; i++) {
        solution_init(&best_solutions[i]);
        starter_packs[i].my_best_solution = &best_solutions[i];
        starter_packs[i].common_stack = common_stack;
        starter_packs[i].common_pool = common_pool;
        starter_packs[i].input_data = &input_data;
    }

    // create and start threads
    pthread_t threads[input_data.t];
    for (int i = 0; i < input_data.t; ++i) {
        pthread_create(&threads[i], NULL, thread_calculation, (void*)&starter_packs[i]);
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

    printf("END POOL FREE SIZE: %d\n", free_list_size(common_pool));

    free(starter_packs);
    stack_destroy(common_stack);
    pool_destroy(common_pool);
    free(best_solutions);

    return 0;
}
