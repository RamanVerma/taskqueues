/*
 * ============================================================================
 *
 *       Filename:  taskqueue.c
 *
 *    Description:  Implementation of the taskqueues 
 *
 *        Version:  1.0
 *        Created:  03/07/2015 04:46:04 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Verma, Raman
 *   Organization:  
 *
 * ============================================================================
 */
#define _GNU_SOURCE
#include"taskqueue.h"
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<string.h>

#define DEBUG 0
#define DEF_S_TQ_SEL_ALGO __round_robin_stq
#define MAX_TQ_NM_LEN 10
struct sigaction act;

sub_taskqueue_t *(* __sel_stq)(taskqueue_t *);
/*
 * __num_CPU            returns the number of CPUs in the system
 *
 * returns the number of CPUs in the system
 * //TODO Is this implementation portable ?
 */
int
__num_CPU()
{
    int num = sysconf(_SC_NPROCESSORS_ONLN);
#if DEBUG
    return 1;
#else
    return num;
#endif /* DEBUG */
 }

/*
 * __create_task_struct creates a task struct from the function and data 
 *      passed as input.
 * @fn                  pointer to the function to be wrapped in task struct
 * @data                pointer to the data to be operated upon
 *
 * returns pointer to the task struct, or NULL in case of any error
 */
static task_t *
__create_task_struct(void(*fn)(void *), void *data)
{
    task_t * t_desc = (task_t *)malloc(sizeof(task_t));
    if(t_desc == NULL) {
        //TODO error message handling
        return NULL;
    }
    t_desc->t_tid = 0;
    t_desc->t_fn = fn;
    t_desc->t_data = data;
    t_desc->t_next = NULL;
    t_desc->t_stq = NULL;
    return t_desc;
}

/*
 * __create_init_flush_struct
 *                      creates and initializes a flush struct
 * @tq_desc             taskqueue structure where the flush struct is to be 
 *                      added later
 *
 * returns a new flush structure that has been initialized
 */
static flush_t *
__create_init_flush_struct(taskqueue_t *tq_desc)
{
    flush_t *f_desc = NULL;
    int index = 0;
    f_desc = (flush_t *)malloc(sizeof(flush_t));
    if(f_desc == NULL) {
        //TODO error message for not allocating memory
        return NULL;
    }
    f_desc->f_id = (int *)malloc(tq_desc->tq_num_stq * sizeof(int));
    if(f_desc->f_id == NULL) {
        //TODO error message for not allocating memory
        free(f_desc);
        return NULL;
    }
    for(index = 0; index < tq_desc->tq_num_stq; index++)
        *(f_desc->f_id + index) = 0;
    pthread_mutex_init(&(f_desc->f_lock), NULL);
    pthread_cond_init(&(f_desc->f_condvar), NULL);
    f_desc->f_num_wake_prereq = 0;
    f_desc->f_next = NULL;
    return f_desc;
}

/*
 * __free_task_struct   frees the task structure passed as an input arg
 * @t_desc              task structure to be freed
 *
 *      NOTE: it does not free the data, as it may be used by other threads.
 *      Also, it does not free *t_next, *t_fn or *t_stq members because those t_data
 *      structures may still be valid.
 */
static void
__free_task_struct(task_t *t_desc)
{
    free(t_desc);
    return;
}

/*
 * __terminal_task      this function is enqueued into a sub_taskqueue by the 
 *      destroy_taskqueue function to facilitate the worker thread to exit 
 *      gracefully.
 *      it practically does nothing but the idea is that a worker thread may
 *      or may not be sleeping on the condition variable(waiting for more 
 *      tasks) when the destroy_taskqueue function is called. since, we cannot
 *      determine the thread's state, we must build the thread's exit logic
 *      in the same workflow that handles the normal task queueing/processing. 
 *      So, we don't need a potentially spurios pthread_cond_signal
 *      call whose behavior is undefined if the thread was not in sleep state.
 * @void *  void pointer for compatibility with general task functions.
 *
 *      this function may be used to handle some cleanup operation though.
 */
static void
__terminal_task(void *p)
{
    return;
}

/*
 * __free_flush_struct  frees the flush structure passed as an input arg
 * @f_desc              flush structure to be freed
 *
 */
static void
__free_flush_struct(flush_t *f_desc)
{
    free(f_desc->f_id);
    pthread_mutex_destroy(&(f_desc->f_lock));
    pthread_cond_destroy(&(f_desc->f_condvar));
    free(f_desc);
}

/*
 * __add_to_flush_list  adds a flush structure to the flush list in the task
 *      queue structure passed as an arg. this function MUST be called after 
 *      locking the flush list for the taskqueue
 * @f_desc              flush structure to be added
 * @tq_desc             taskqueue where the flush struct is to be added
 */
static void
__add_to_flush_list(flush_t *f_desc, taskqueue_t *tq_desc)
{
    if(tq_desc->tq_flushlist_tail == NULL) {
        tq_desc->tq_flushlist_tail = f_desc;
        tq_desc->tq_flushlist_head = f_desc;
    }else {
        tq_desc->tq_flushlist_tail->f_next = f_desc;
        tq_desc->tq_flushlist_tail = f_desc;
    }
}

/*
 * __remove_from_flush_list
 *                      finds a flush structure in the flush list and removes
 *      the same. since multiple flush structures might be waiting on the same
 *      set of tasks across the taskqueue, and will therefore be signalled 
 *      almost at the same time. hence, we cannot just remove the head element
 *      of this list, but rather we must traverse the list from head to tail 
 *      to find the correct structure to be removed.
 *      this function MUST be called after locking the flush list for the 
 *      taskqueue
 * @f_desc              flush structure to be removed
 * @tq_desc             taskqueue to which the flush list belongs
 */
static void
__remove_from_flush_list(flush_t *f_desc, taskqueue_t *tq_desc)
{
    flush_t *f_current = tq_desc->tq_flushlist_head;
    flush_t *f_prev = tq_desc->tq_flushlist_head;
    while(f_current != NULL) {
        if(f_current == f_desc)
            break;
        f_prev = f_current;
        f_current = f_current->f_next;
    }
    if(f_current == NULL) {
        //TODO flush list empty, or
        //TODO the flush struct is not found in the queue! error handling
        return;
    }
    if(f_current == tq_desc->tq_flushlist_head) {
        tq_desc->tq_flushlist_head = tq_desc->tq_flushlist_head->f_next;
    }else {
        if(f_current == tq_desc->tq_flushlist_tail) {
            tq_desc->tq_flushlist_tail = f_prev;
            f_prev->f_next = NULL;
        }else {
            f_prev = f_current->f_next;
        }
    }
    __free_flush_struct(f_current);
    if(tq_desc->tq_flushlist_head == NULL)
        tq_desc->tq_flushlist_tail = NULL;
    return;
}

/*
 * __get_flush_reqs     find the tail tasks in all the sub taskqueues
 *      belonging to a taskqueue and add their ids to the flush struct. these 
 *      tasks need to be completed before a thread waiting on a call to 
 *      flush_taskqueue wakes. also, updates the count for total number of 
 *      dependencies for the waiting thread
 * @f_desc              pointer to the flush structure to be filled
 * @tq_desc             pointer to the taskqueue structure
 *
 */
static void
__get_flush_reqs(flush_t *f_desc, taskqueue_t *tq_desc)
{
    int index = 0;
    sub_taskqueue_t *stq = NULL;
    for(index = 0; index < tq_desc->tq_num_stq; index++) {
        stq = tq_desc->tq_stq + index;
        pthread_mutex_lock(&(stq->s_tasklist_lock));
        if(stq->s_tasklist_tail != NULL) {
            *(f_desc->f_id + index) = stq->s_tasklist_tail->t_tid;
            (f_desc->f_num_wake_prereq)++;
        }
        pthread_mutex_unlock(&(stq->s_tasklist_lock));
    }
}

/*
 * __check_flush_queue    checks if a tisk id is present in the flush list 
 *      associated with a task queue. if present, the f_tid is set to 0,
 *      f_num_wake_prereq is decremented by 1, and if f_num_wake_prereq becomes
 *      0, signal the thread waiting on corresponding f_condvar variable
 * @tq_desc             taskqueue structure where the flush list resides
 * @stq_id             id of the sub task queue to which the task belongs
 * @tid                 id of the task to be searched
 */
static void
__check_flush_queue(taskqueue_t *tq_desc, int stq_id, int tid)
{
    flush_t *f_desc_next = tq_desc->tq_flushlist_head;
    flush_t *f_desc = NULL;
    while((f_desc = f_desc_next) != NULL) {
        if(*(f_desc->f_id + stq_id) != tid)
            break;
        pthread_mutex_lock(&(f_desc->f_lock));
        f_desc->f_num_wake_prereq--;
        *(f_desc->f_id + stq_id) = 0;
        f_desc_next = f_desc->f_next;
        //FIXME do we need to unlock f_desc->f_lock before signalling 
        // the process waiting on cond var, and continue. Double Check !
        // could lead to seg fault if f_desc is released by waiting process
        // and we call unlock after that.
        if(f_desc->f_num_wake_prereq == 0)
            pthread_cond_signal(&(f_desc->f_condvar));
        pthread_mutex_unlock(&(f_desc->f_lock));
    }
}

/*
 * __round_robin_stq   returns the sub taskqueue according to a round 
 *      robin scheme
 * @tq_desc             task queue to be investigated
 *
 * this function MUST be called in a synchronized manner in case when we have
 * multiple sub taskqueues per taskqueue
 * returns the sub taskqueue structure that contains least number of tasks
 */
static sub_taskqueue_t *
__round_robin_stq(taskqueue_t * tq_desc)
{
    tq_desc->tq_next_rr_stq = 
    	(++tq_desc->tq_next_rr_stq)%(tq_desc->tq_num_stq);
    return tq_desc->tq_stq + tq_desc->tq_next_rr_stq;
}

/*
 * __select_stq    selects a sub taskqueue, according to the selection
 *      algorithm set for the taskqueue structure. 
 * @tq_desc         pointer to the taskqueue structure
 *
 * returns a pointer to the selected sub taskqueue structure
 */
static sub_taskqueue_t *
__select_stq(taskqueue_t * tq_desc)
{
    sub_taskqueue_t *stq = NULL;
    switch(tq_desc->tq_stq_sel_algo) {
        case 0: 
            __sel_stq = DEF_S_TQ_SEL_ALGO;
            break;
        default: 
            __sel_stq = DEF_S_TQ_SEL_ALGO;
            break;
    }
    //FIXME we should use tq_stq_sel_lock here
    stq = __sel_stq(tq_desc);
    return stq;
}

/*
 * __add_task_to_stq    add a task structure to the sub taskqueue.
 *      corresponding lock of the sub_taskqueue MUST be acquired by the 
 *      calling function.
 * @stq                 sub_taskqueue where the task has to be added
 * @t_desc              pointer to the task to be added
 */
static void
__add_task_to_stq(sub_taskqueue_t *stq, task_t *t_desc)
{
    if(stq->s_tasklist_tail != NULL)
        t_desc->t_tid = (stq->s_tasklist_tail->t_tid) + 1;
    else
        t_desc->t_tid = 1;
    t_desc->t_stq = stq;
    if(t_desc->t_tid < 1)
        t_desc->t_tid = 1;
    if(stq->s_tasklist_tail != NULL) {
        stq->s_tasklist_tail->t_next = t_desc;
        stq->s_tasklist_tail = t_desc;
    }else {
        stq->s_tasklist_head = t_desc;
        stq->s_tasklist_tail = t_desc;
    }
    stq->s_num_tasks++;
    if(stq->s_num_tasks == 1)
        pthread_cond_signal(&(stq->s_more_task));
}

/*
 * __close_stq_add_term_task
 *                      closes a sub_taskqueue for any more tasks to be added.
 *      adds the terminal task to sub_taskqueue internally. terminal task 
 *      facilitates worker thread to exit gracefully.
 * @stq                 sub_taskqueue to operate on
 *
 * returns 0; if terminal task could not be added to sub taskqueue
 *         1; if terminal task has been added and the caller should issue a 
 *         pthread_join for the worker thread.
 *         2; if sub_taskqueue has already been closed by some one else
 */
static int
__close_stq_add_term_task(sub_taskqueue_t *stq)
{
    task_t *t_desc = NULL;
    pthread_mutex_lock(&(stq->s_tasklist_lock));
    if(stq->s_id < 0) {
        pthread_mutex_unlock(&(stq->s_tasklist_lock));
        return 2;
    }
    stq->s_id = -1;
    t_desc = __create_task_struct(__terminal_task, NULL);
    if(t_desc == NULL) {
        //TODO possible memory leak case: we make sure that we do not wait
        //for this thread to join. Note: the worker may, or may not be running.
        //If worker is running at this stage, it will finally jump out of its
        //infinite loop and die. If it is sleeping right now, it will stay
        //asleep. Caller functions will also not release the corresponding
        //sub_taskqueues, and the memory will only be freed when main dies.
        //Test these scenarios.
        //TODO log error message
        pthread_mutex_unlock(&(stq->s_tasklist_lock));
        return 0;
    }
    __add_task_to_stq(stq, t_desc);
    pthread_mutex_unlock(&(stq->s_tasklist_lock));
    return 1;
}

/*
 * __stq_graceful_exit  signal handler to gracefully close a sub_taskqueue.
 *      1. mark the sub_taskqueue un usable, so that user cannot queue any
 *      more tasks in it,
 *      2. queue a terminal task into the sub taskqueue internally. it will 
 *      help the worker thread to wake, if it was waiting on more_tasks cond
 *      var
 *      2.1 with the sub taskqueue already marked un usable and this being the 
 *      last task, the worker thread exits.
 *      3. wait on the sub_taskqueue thread to join
 * @stq                 pointer to the sub_taskqueue to be exited
 *
 *      design philosophy: we let all the tasks already enqueued to be 
 *      completed, before we gracefully exit the worker thread.
 */
static void
__stq_graceful_exit(sub_taskqueue_t *stq)
{
    taskqueue_t *tq_desc = stq->s_parent_tq;
    int wait_for_worker = 0;
    void *status;
    //TODO Do I really need flushlist_lock ?
    pthread_mutex_lock(&(tq_desc->tq_flushlist_lock));
    wait_for_worker = __close_stq_add_term_task(stq);
    pthread_mutex_unlock(&(tq_desc->tq_flushlist_lock));
    if(wait_for_worker == 1) {
        pthread_join(stq->s_worker, &status);
        pthread_cond_destroy(&(stq->s_more_task));
        pthread_mutex_destroy(&(stq->s_worker_lock));
        pthread_mutex_destroy(&(stq->s_tasklist_lock));
        free(stq);
        pthread_mutex_lock(&(tq_desc->tq_count_good_stq_lock));
        (tq_desc->tq_good_stq)--;
        pthread_mutex_unlock(&(tq_desc->tq_count_good_stq_lock));
    }
    //FIXME still cannot free taskqueue struct because even though all the 
    //sub_taskqueue workers have exited. There may be a thread waiting on a 
    //flush and it may still have not ben scheduled to run. If we free tq_desc,
    //those threads will SIGSEGV when they try to acquire tq_flushlist_lock.
    //Potential solution is to add a flsuh call here, but make sure that flush
    //struct will be signalled only after every other flush structure has been
    //signalled.
}

/*
 * __worker_thread      function executed by each worker thread. 
 *      each thread runs in the following loop 
 *      1. acquire the mutex in the sub taskqueue 
 *      2. check the number of tasks pending in this taskqueue
 *          2.1 sleep on the condition variable if there are no task to 
 *          execute else, 
 *          2.2 if s_id < 0 && s_num_tasks = 0, break from the loop, 
 *          call pthread_exit
 *          2.3 remove a task from the head of task linked list and 
 *          reduce the number of pending tasks in the taskqueue 
 *      3. release the mutex acquired earlier
 *      4. execute the task by calling the registered function
 *      5. free up the task structure.
 * @data                data for the thread to work upon, passed as a void 
 *      pointer
 */
static void *
__worker_thread(void *data)
{
    sub_taskqueue_t *stq = (sub_taskqueue_t *)data;
    task_t *t_desc = NULL;
    int tid = 0;
    while(1) {
        pthread_mutex_lock(&(stq->s_worker_lock));
        if(stq->s_id < 0 && stq->s_num_tasks == 0) {
            pthread_mutex_unlock(&(stq->s_worker_lock));
            break;
        }
        if(stq->s_num_tasks == 0)
            pthread_cond_wait(&(stq->s_more_task), &(stq->s_worker_lock));
        t_desc = stq->s_tasklist_head;
        //TODO We cannot return anything here. Seems right though.
        t_desc->t_fn(t_desc->t_data);
        pthread_mutex_lock(&(stq->s_tasklist_lock));
        stq->s_tasklist_head = stq->s_tasklist_head->t_next;
        if(stq->s_tasklist_head == NULL)
            stq->s_tasklist_tail = NULL; 
        stq->s_num_tasks--; 
        pthread_mutex_unlock(&(stq->s_tasklist_lock));
        //TODO this function should ideally be a separate thread
        tid = t_desc->t_tid;
        __check_flush_queue(stq->s_parent_tq, stq->s_id, tid);
        //TODO Test that it does not release t_data or stq structures
        free(t_desc);
        pthread_mutex_unlock(&(stq->s_worker_lock));
    }
    pthread_exit(0);
}

/*
 * __init_n_stq     initialize n sub_taskqueues
 * @tq_desc         pointer to the parent taskqueue
 * @n               number of sub_taskqueues to be initialized
 *
 * NOTE: the thread name string is set to max 16 bytes, as per prctl() help
 */
static void
__init_n_stq(taskqueue_t *tq_desc, int n)
{
    pthread_attr_t attr;
    char name[16] = {'\0'};
    sub_taskqueue_t *stq = NULL;
    int index = 0;
    tq_desc->tq_good_stq = n;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    for(index = 0; index < n; index++) {
        stq = (tq_desc->tq_stq + index);
        pthread_mutex_init(&(stq->s_tasklist_lock), NULL);
        pthread_mutex_init(&(stq->s_worker_lock), NULL);
        pthread_cond_init(&(stq->s_more_task), NULL);
        stq->s_id = index;
        stq->s_tasklist_head = NULL;
        stq->s_tasklist_tail = NULL;
        stq->s_num_tasks = 0;
        stq->s_parent_tq = tq_desc; 
        pthread_create(&(stq->s_worker), &attr, __worker_thread, (void *)stq);
        snprintf(name, sizeof(name), "%s/%d", tq_desc->tq_id, index);
        pthread_setname_np(stq->s_worker, name);
    }
    pthread_attr_destroy(&attr);
}

/*
 * create_custom_taskqueue     
 *                      creates a taskqueue with the name tq_name. It creates
 *      'n' worker threads. Threads are named after the taskqueue name passed 
 *      as a parameter. sub taskqueue selection algorithm is specified as the
 *      third parameter.
 * @tq_name             name for the taskqueue to be created, upto 10 char
 * @n                   number of sub taskqueues to be created
 * @stq_sel_algo        algorithm to select the sub taskqueue for queueing a
 *      task
 *
 * returns a pointer to the new taskqueue structure, NULL in case of an error
 */
taskqueue_t *
create_custom_taskqueue(char *tq_name, int n, int stq_sel_algo)
{
    taskqueue_t *tq_desc = (taskqueue_t *)malloc(sizeof(taskqueue_t));
    if(tq_desc == NULL) {
        //TODO error handling
        return NULL;
    }
    pthread_mutex_init(&(tq_desc->tq_count_good_stq_lock), NULL);
    pthread_mutex_init(&(tq_desc->tq_stq_sel_lock), NULL);
    pthread_mutex_init(&(tq_desc->tq_flushlist_lock), NULL);
    pthread_cond_init(&(tq_desc->tq_yield_to_flush_structs_cond), NULL);
    tq_desc->tq_marked_for_destruction = 0;
    tq_desc->tq_flushlist_head = NULL;
    tq_desc->tq_flushlist_tail = NULL;
    tq_desc->tq_next_rr_stq = 0;
    tq_desc->tq_stq_sel_algo = stq_sel_algo;
    tq_desc->tq_num_stq = n;
    tq_desc->tq_id = (char *)malloc((MAX_TQ_NM_LEN + 1) * sizeof(char));
    if (tq_desc->tq_id) {
        tq_desc->tq_id[0] = '\0';
        if (tq_name) {
            strncpy(tq_desc->tq_id, tq_name, MAX_TQ_NM_LEN);
            tq_desc->tq_id[strlen(tq_desc->tq_id) - 1] = '\0';
        }
    }
    tq_desc->tq_stq = (sub_taskqueue_t *)malloc(n * sizeof(sub_taskqueue_t));
    if(tq_desc->tq_stq == NULL) {
        //TODO error handling
        free(tq_desc);
        return NULL;
    }
    __init_n_stq(tq_desc, n);
    return tq_desc;
}

/*
 * create_taskqueue     creates a taskqueue with the name tq_name. It creates
 *      'n' worker threads, one for each of the CPUs in the system. Threads are
 *      named after the taskqueue name passed as a parameter. default sub 
 *      taskqueue selection algorithm is used.
 * @tq_name             name for the taskqueue to be created
 *
 * returns a pointer to the taskqueue structure that describes the task queue
 */
taskqueue_t *
create_taskqueue(char *tq_name)
{
    return create_custom_taskqueue(tq_name, __num_CPU(), 0); 
}

/*
 * create_singlethread_taskqueue
 *                      creates a task queue just like the create_taskqueue 
 *      function but single threaded, no matter how many CPUs are present in 
 *      the system.
 * @tq_name             name for the taskqueue to be created
 *
 * returns a pointer to the taskqueue structure that describes the task queue
 */
taskqueue_t *
create_singlethread_taskqueue(char *tq_name)
{
    return create_custom_taskqueue(tq_name, 1, 0); 
}

/*
 * destroy_taskqueue    destroys a taskqueue. the user application MUST NOT 
 *      make any further operations on the taskqueue, once this function has 
 *      been called.
 *      1. mark all the sub_taskqueues un usable, so that user cannot queue any
 *      more tasks in them, 
 *      2. queue a terminal task into the sub taskqueues internally. it will 
 *      help the worker thread to wake, if it was waiting on more_tasks cond
 *      var
 *      2.1 with the sub taskqueue already marked un usable and this being the 
 *      last task, the worker thread exits.
 *      3. wait on all the sub_taskqueues threads to join
 * @tq_desc             pointer to the taskqueue_struct to be destroyed
 *
 *      design philosophy: we let all the tasks already enqueued to be 
 *      completed, before we gracefully exit the worker thread.
 */
void
destroy_taskqueue(taskqueue_t *tq_desc)
{
    int index = 0;
    sub_taskqueue_t *stq = NULL;
    int wait_for_worker[tq_desc->tq_num_stq];
    void *status;
    for(index = 0; index < tq_desc->tq_num_stq; index++)
        wait_for_worker[index] = 0;

    /* synchronize with the flush mechanism */
    pthread_mutex_lock(&(tq_desc->tq_flushlist_lock));
    tq_desc->tq_marked_for_destruction = 1;
    pthread_mutex_unlock(&(tq_desc->tq_flushlist_lock));
    /* add a terminal task to each sub_taskqueue */
    for(index = 0; index < tq_desc->tq_num_stq; index++) {
        wait_for_worker[index] = 
            __close_stq_add_term_task(tq_desc->tq_stq + index);
    }
    /* wait for all the sub taskqueues threads to finish */
    for(index = (tq_desc->tq_num_stq - 1); index >= 0; index--) {
        if(wait_for_worker[index] == 1) {
            stq = tq_desc->tq_stq + index;
            pthread_join(stq->s_worker, &status);
            pthread_cond_destroy(&(stq->s_more_task));
            pthread_mutex_destroy(&(stq->s_worker_lock));
            pthread_mutex_destroy(&(stq->s_tasklist_lock));
            free(stq);
            pthread_mutex_lock(&(tq_desc->tq_count_good_stq_lock));
            (tq_desc->tq_good_stq)--;
            pthread_mutex_unlock(&(tq_desc->tq_count_good_stq_lock));
        }
    }
    /* wait on all the flush structs to be freed gracefully */
    pthread_mutex_lock(&(tq_desc->tq_flushlist_lock));
    if (tq_desc->tq_flushlist_head != NULL) {
        pthread_cond_wait(&(tq_desc->tq_yield_to_flush_structs_cond), 
                            &(tq_desc->tq_flushlist_lock));
    }
    pthread_mutex_unlock(&(tq_desc->tq_flushlist_lock));
    /* finally */
    pthread_mutex_destroy(&(tq_desc->tq_count_good_stq_lock));
    pthread_mutex_destroy(&(tq_desc->tq_stq_sel_lock));
    pthread_mutex_destroy(&(tq_desc->tq_flushlist_lock));
    pthread_cond_destroy(&(tq_desc->tq_yield_to_flush_structs_cond));
    free(tq_desc->tq_id);
    free(tq_desc);
}

/*
 * queue_task           queues a task in a task queue.
 * @tq_desc             pointer to the task queue where task is to be queued
 * @fn                  pointer to the function to be executed by the task
 * @data                data to be operated upon by the function
 *
 *      create a task structure to be queued. select a sub_taskqueue. 
 *      this sub_taskqueue has to be checked for being valid/usable,
 *      (s_id >= 0) because there is a chance that the user calls destroy_
 *      taskqueue, or some one kills this thread soon after the sub_taskqueue 
 *      gets selected.
 *      Once the task has been added to the task queue, the function wakes any 
 *      worker thread sleeping on the more_task wait queue in the local CPU's 
 *      sub_taskqueue decriptor.
 *
 * returns 0, if task is added to the task queue
 *         1, if task was already present in the task queue
 *        -1, if there is no more usable sub_taskqueue
 *        -2, in case of any errors
 */
int
queue_task(taskqueue_t *tq_desc, void(* fn)(void *), void *data)
{
    task_t *t_desc = __create_task_struct(fn, data);
    if(t_desc == NULL) {
        //TODO log error message
        return -2;
    }
    sub_taskqueue_t *stq = NULL;
    do {
        stq = __select_stq(tq_desc);
        pthread_mutex_lock(&(stq->s_tasklist_lock));
        if(stq->s_id < 0)
            pthread_mutex_unlock(&(stq->s_tasklist_lock));
    } while(stq->s_id < 0 && tq_desc->tq_good_stq > 0);
    if(tq_desc->tq_good_stq == 0) {
        pthread_mutex_unlock(&(stq->s_tasklist_lock));
        return -1;
    }
    __add_task_to_stq(stq, t_desc);
    pthread_mutex_unlock(&(stq->s_tasklist_lock));
    return 0;
}

/*
 * queue_delayed_task   this function is similar to queue_task except that it 
 *      adds a task in the sub task queue after a certain delay specified
 *      as the third parameter to the function.
 * @tq_desc             pointer to the task queue where task is to be queued
 * @fn                  pointer to the function to be executed by the task
 * @data                data to be operated upon by the function
 * @delay               delay in <> after which the task is to be added to the
 *      sub task queue descriptor.
 *
 * returns 0, if task is scheduled to be added the sub task queue
 *         1, if task was already present in the task queue
 *        -1, in case of any errors
 */
int
queue_delayed_task(taskqueue_t * tq_desc, void(* fn)(void *),
                       void *data, unsigned long delay)
{
    return 0;
}

/*
 * cancel_delayed_task  cancels a task that was scheduled to be added to a sub
 *      taskqueue after a delay. If the task has already been added to the
 *      sub taskqueue, the function returns.
 * @t_desc              descriptor for the task to be cancelled
 *
 * returns 0, upon successfully cancelling a task
 *         1, if the task has already been added to the sub task queue
 *         -1, in case of any error
 */
int
cancel_delayed_task(task_t *t_desc)
{
    return 0;
}

/*
 * flush_taskqueue      blocks until all the tasks in a task queue descriptor
 *      are executed. Any task added after the call to flush_taskqueue are not
 *      considered.
 * @tq_desc             pointer to the taskqueue descriptor
 *
 * returns 0, upon sucess and -1 on failure
 */
int
flush_taskqueue(taskqueue_t *tq_desc)
{
    pthread_mutex_lock(&(tq_desc->tq_flushlist_lock));
    flush_t *f_desc = NULL;
    f_desc = __create_init_flush_struct(tq_desc);
    if(f_desc == NULL) {
        pthread_mutex_unlock(&(tq_desc->tq_flushlist_lock));
        return -1;
    }
    pthread_mutex_lock(&(f_desc->f_lock));
    __get_flush_reqs(f_desc, tq_desc);
    if(f_desc->f_num_wake_prereq == 0) {
        pthread_mutex_unlock(&(f_desc->f_lock));
        __free_flush_struct(f_desc);
        pthread_mutex_unlock(&(tq_desc->tq_flushlist_lock));
        return 0;
    }else {
        __add_to_flush_list(f_desc, tq_desc);
        pthread_mutex_unlock(&(tq_desc->tq_flushlist_lock));
        pthread_cond_wait(&(f_desc->f_condvar), &(f_desc->f_lock));
        pthread_mutex_unlock(&(f_desc->f_lock));
    }
    pthread_mutex_lock(&(tq_desc->tq_flushlist_lock));
    __remove_from_flush_list(f_desc, tq_desc);
    if (tq_desc->tq_marked_for_destruction && !tq_desc->tq_flushlist_head)
        pthread_cond_signal(&(tq_desc->tq_yield_to_flush_structs_cond));
    pthread_mutex_unlock(&(tq_desc->tq_flushlist_lock));
    return 0;
}

/*
 * signal handling 
 * FIXME pass the correct sub taskqueue to the handler
 */
//memset(&act, 0, sizeof(act));
//act.sa_sigaction = __stq_graceful_exit;
//act.sa_flags = SA_SIGINFO;
//sigaction(SIGTERM, &act, NULL);
