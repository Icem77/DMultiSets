#include <stddef.h>

#include "common/io.h"
#include "common/sumset.h"

#include <pthread.h>
#include <stdatomic.h>

#include <stdlib.h>
#include <stdio.h>

#define INITIAL_BRANCH_POOL_SIZE 8192
#define INITIAL_SUMSET_POOL_SIZE 1024

atomic_int locks = 0;
atomic_int frees = 0;
int max_sps_use = 0;

int max(int a, int b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

typedef struct SmartParallelSumset {
    Sumset sumset;
    struct SmartParallelSumset* parent;

    atomic_int parent_to;

    struct SmartParallelSumset* next_on_free_list;
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

typedef struct SPSPool {
    SPS_t* pool;
    SPS_t* free_list;
    int pool_size;

    pthread_mutex_t mutex;
} SPSPool_t;

typedef struct ThreadResources {
    BranchPool_t* branch_pool;
    InputData* input;
    Solution* mySolution;
    SPSPool_t* sps_pool;
} TR_t;

SPSPool_t* sps_pool_init() {
    SPSPool_t* sps_pool = (SPSPool_t*) malloc(sizeof(SPSPool_t));
    sps_pool->pool = (SPS_t*) malloc(INITIAL_SUMSET_POOL_SIZE * sizeof(SPS_t));
    sps_pool->pool_size = INITIAL_SUMSET_POOL_SIZE;

    for (int i = 0; i < sps_pool->pool_size - 1; ++i) {
        sps_pool->pool[i].next_on_free_list = &sps_pool->pool[i + 1];
    }
    sps_pool->pool[sps_pool->pool_size - 1].next_on_free_list = NULL;

    sps_pool->free_list = &sps_pool->pool[0];

    pthread_mutex_init(&sps_pool->mutex, NULL);

    return sps_pool;
}

SPS_t* sps_pool_get(SPSPool_t* pool) {
    pthread_mutex_lock(&pool->mutex);
    SPS_t* to_return = pool->free_list;
    pool->free_list = pool->free_list->next_on_free_list;
    pthread_mutex_unlock(&pool->mutex);
    return to_return;
}

void sps_pool_return(SPSPool_t* pool, SPS_t* returning) {
    pthread_mutex_lock(&pool->mutex);
    returning->next_on_free_list = pool->free_list;
    pool->free_list = returning;
    pthread_mutex_unlock(&pool->mutex);
}

void sps_pool_destroy(SPSPool_t* pool) {
    free(pool->pool);
    pthread_mutex_destroy(&pool->mutex);
    free(pool);
}

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

void check_if_free(SPSPool_t* pool, SPS_t* a) {
    if (atomic_fetch_sub(&a->parent_to, 1) == 1) {
        SPS_t* parent_to_check = a->parent;
        atomic_fetch_add(&frees, 1);
        sps_pool_return(pool, a);
        check_if_free(pool, parent_to_check);
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
                max_sps_use = max(max_sps_use, atomic_load(&locks) - atomic_load(&frees));
                SPS_t* a_with_i = sps_pool_get(resources->sps_pool);
                a_with_i->parent = a;
                atomic_store(&a_with_i->parent_to, 1);

                sumset_add(&a_with_i->sumset, &a->sumset, i);

                atomic_fetch_add(&a->parent_to, 1);
                atomic_fetch_add(&b->parent_to, 1);
                give_away_branch(resources->branch_pool, a_with_i, b);
            }
        }
    } else if ((a->sumset.sum == b->sumset.sum) && (get_sumset_intersection_size(&a->sumset, &b->sumset) == 2)) { // s(a) ∩ s(b) = {0, ∑b}.
        if (a->sumset.sum > resources->mySolution->sum) {
            solution_build(resources->mySolution, resources->input, &a->sumset, &b->sumset);
        }
    }

    check_if_free(resources->sps_pool, a);
    check_if_free(resources->sps_pool, b);
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
            check_if_free(resources->sps_pool, a);
            check_if_free(resources->sps_pool, b);
        }

        take_new_branch(resources->branch_pool, &a, &b, resources->input->t);
    }

    return NULL;
}

int main()
{   
    InputData input_data;
    //input_data_read(&input_data);
    input_data_init(&input_data, 12, 28, (int[]){0}, (int[]){1, 0});

    BranchPool_t* common_branch_pool = branch_pool_init();
    SPSPool_t* sps_pool = sps_pool_init();

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
        starterPacks[i].sps_pool = sps_pool;
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
    sps_pool_destroy(sps_pool);

    if (locks == frees) printf("ALL GOOD!\n");
    printf("MAX SPS USE: %d\n", max_sps_use);
    
    return 0;
}
