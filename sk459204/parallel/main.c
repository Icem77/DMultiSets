#include <stddef.h>

#include "common/io.h"
#include "common/sumset.h"

#include <pthread.h>
#include <stdatomic.h>

#include <stdlib.h>
#include <stdio.h>

#define INITIAL_BRANCH_POOL_SIZE 8192

atomic_int locks = 0;
atomic_int frees = 0;

typedef struct SmartParallelSumset {
    Sumset sumset;
    struct SmartParallelSumset* parent;

    atomic_int parent_to;
} SPS_t;

typedef struct BranchPool {
    SPS_t** stack;
    int last_push_index;
    int stack_size;

    pthread_mutex_t mutex;

    pthread_cond_t waiting_room;
    int waiting_threads;

    bool finish;
} BranchPool_t;

typedef struct ThreadResources {
    BranchPool_t* branch_pool;
    InputData* input;
    Solution* mySolution;
} TR_t;

BranchPool_t* branch_pool_init() {
    BranchPool_t* pool = (BranchPool_t*) malloc(sizeof(BranchPool_t));

    pool->stack = (SPS_t**) malloc(INITIAL_BRANCH_POOL_SIZE * sizeof(SPS_t*));
    pool->last_push_index = -1;
    pool->stack_size = INITIAL_BRANCH_POOL_SIZE;

    pthread_mutex_init(&pool->mutex, NULL);

    pthread_cond_init(&pool->waiting_room, NULL);
    pool->waiting_threads = 0;

    pool->finish = false;

    return pool;
}

void give_away_branch(BranchPool_t* pool, SPS_t* a, SPS_t* b) {
    pthread_mutex_lock(&pool->mutex);
    pool->last_push_index += 2;
    pool->stack[pool->last_push_index - 1] = a;
    pool->stack[pool->last_push_index] = b;
    pthread_cond_signal(&pool->waiting_room);
    pthread_mutex_unlock(&pool->mutex);
}

void take_new_branch(BranchPool_t* pool, SPS_t** a, SPS_t** b, int workingThreads) {
    pthread_mutex_lock(&pool->mutex);

    *a = NULL;
    *b = NULL;

    if (pool->last_push_index == -1 && pool->waiting_threads == workingThreads - 1) {
        pool->finish = true;
        pthread_cond_broadcast(&pool->waiting_room);
        pthread_mutex_unlock(&pool->mutex);
        return;
    } else if (pool->last_push_index == -1) {
        while (pool->last_push_index == -1 && !pool->finish) {
            pool->waiting_threads++;
            pthread_cond_wait(&pool->waiting_room, &pool->mutex);
            pool->waiting_threads--;
        }
    }

    if (pool->last_push_index != -1) {
        *b = pool->stack[pool->last_push_index];
        *a = pool->stack[pool->last_push_index - 1];
        pool->last_push_index -= 2;
    }

    pthread_mutex_unlock(&pool->mutex);
}

int branch_pool_size(BranchPool_t* pool) {
    pthread_mutex_lock(&pool->mutex);
    int result = (pool->last_push_index + 1) / 2;
    pthread_mutex_unlock(&pool->mutex);
    return result;
}

void branch_pool_destroy(BranchPool_t* pool) {
    free(pool->stack);
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->waiting_room);
    free(pool);
}

void swap(SPS_t** a, SPS_t** b) {
    SPS_t* tmp = *a;
    *a = *b;
    *b = tmp;
}

void check_if_free(SPS_t* a) {
    if (atomic_fetch_sub(&a->parent_to, 1) == 1) {
        SPS_t* parent_to_check = a->parent;
        atomic_fetch_add(&frees, 1);
        free(a);
        check_if_free(parent_to_check);
    }    
}

void branch_split(TR_t* resources, SPS_t* a, SPS_t* b) {
    if (a->sumset.sum > b->sumset.sum) {
        swap(&a, &b);
    }

    if (is_sumset_intersection_trivial(&a->sumset, &b->sumset)) { // s(a) ∩ s(b) = {0}.
        for (size_t i = a->sumset.last; i <= resources->input->d; ++i) {
            if (!does_sumset_contain(&b->sumset, i)) {
                atomic_fetch_add(&locks, 1);
                SPS_t* a_with_i = (SPS_t*) malloc(sizeof(SPS_t));
                a_with_i->parent = a;
                atomic_store(&a_with_i->parent_to, 1);

                sumset_add(&a_with_i->sumset, &a->sumset, i);

                atomic_fetch_add(&a->parent_to, 1);
                atomic_fetch_add(&b->parent_to, 1);
                give_away_branch(resources->branch_pool, a_with_i, b);
            }
        }
        // zliczamy sie w lisciach 
        check_if_free(a);
        check_if_free(b);
    } else {
        if ((a->sumset.sum == b->sumset.sum) && (get_sumset_intersection_size(&a->sumset, &b->sumset) == 2)) { // s(a) ∩ s(b) = {0, ∑b}.
            if (a->sumset.sum > resources->mySolution->sum) {
                solution_build(resources->mySolution, resources->input, &a->sumset, &b->sumset);
            }
        }
        check_if_free(a);
        check_if_free(b);
    } 
}

void recursive_solv(TR_t* resources, SPS_t* a, SPS_t* b) {
    if (a->sumset.sum > b->sumset.sum) {
        recursive_solv(resources, b, a);
    } else {
        if (is_sumset_intersection_trivial(&a->sumset, &b->sumset)) { // s(a) ∩ s(b) = {0}.
            for (size_t i = a->sumset.last; i <= resources->input->d; ++i) {
                if (!does_sumset_contain(&b->sumset, i)) {
                    SPS_t a_with_i;
                    a_with_i.parent = a;
                    sumset_add(&a_with_i.sumset, &a->sumset, i);
                    recursive_solv(resources, &a_with_i, b);
                }
            }
        } else if ((a->sumset.sum == b->sumset.sum) && (get_sumset_intersection_size(&a->sumset, &b->sumset) == 2)) { // s(a) ∩ s(b) = {0, ∑b}.
            if (b->sumset.sum > resources->mySolution->sum)
                solution_build(resources->mySolution, resources->input, &a->sumset, &b->sumset);
        }    
    }
}

void* thread_calculations(void* args) {
    TR_t* resources = (TR_t*) args;

    SPS_t* a;
    SPS_t* b;

    take_new_branch(resources->branch_pool, &a, &b, resources->input->t);

    while (a != NULL && b != NULL) {
        if (branch_pool_size(resources->branch_pool) < resources->input->t - 1) {
            branch_split(resources, a, b);
        } else {
            recursive_solv(resources, a, b);
            check_if_free(a);
            check_if_free(b);
        }

        take_new_branch(resources->branch_pool, &a, &b, resources->input->t);
    }

    return NULL;
}

int main()
{   
    InputData input_data;
    //input_data_read(&input_data);
    input_data_init(&input_data, 8, 34, (int[]){0}, (int[]){1, 0});

    BranchPool_t* common_branch_pool = branch_pool_init();

    SPS_t a;
    a.sumset = input_data.a_start;
    a.parent = NULL;
    atomic_store(&a.parent_to, 7);

    SPS_t b;
    b.sumset = input_data.b_start;
    b.parent = NULL;
    atomic_store(&b.parent_to, 7);

    give_away_branch(common_branch_pool, &a, &b);

    Solution solutions[input_data.t];
    TR_t starterPacks[input_data.t];

    for (int i = 0; i < input_data.t; ++i) {
        solution_init(&solutions[i]);
        starterPacks[i].branch_pool = common_branch_pool;
        starterPacks[i].input = &input_data;
        starterPacks[i].mySolution = &solutions[i];
    }

    pthread_t threads[input_data.t];
    for (int i = 0; i < input_data.t; ++i) {
        pthread_create(&threads[i], NULL, thread_calculations, &starterPacks[i]);
    }

    for (int i = 0; i < input_data.t; ++i) {
        pthread_join(threads[i], NULL);
    }
    
    Solution* best_solution = &solutions[0];
    for (int i = 1; i < input_data.t; ++i) {
        if (solutions[i].sum > best_solution->sum) {
            best_solution = &solutions[i];
        }
    }

    solution_print(best_solution);

    branch_pool_destroy(common_branch_pool);

    printf("LOCKS: %d - FREES: %d", locks, frees);
    
    return 0;
}
