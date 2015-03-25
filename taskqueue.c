/*
 * ============================================================================
 *
 *       Filename:  taskqueue.c
 *
 *    Description:  
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
#include"taskqueue.h"
#include<unistd.h>
#include<stdlib.h>

#define DEBUG 0
#define PCTQ_SEL_ALGO round_robin_pctq

struct percpu_taskqueue_struct *(* select_pctq)(struct taskqueue_struct *);
/*
 * num_CPU              returns the number of CPUs in the system
 *
 * returns the number of CPUs in the system
 * //TODO Is this implementation portable ?
 */
 int num_CPU(){
    int num = sysconf(_SC_NPROCESSORS_ONLN);
#if DEBUG
    return 2;
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
struct task_struct *__create_task_struct(void(*fn)(void *), void *data){
    struct task_struct * t_desc =
                    (struct task_struct *)malloc(sizeof(struct task_struct));
    if(t_desc == NULL){
        //TODO error message handling
        return NULL;
    }
    t_desc->task_id = 0;
    t_desc->fn = fn;
    t_desc->data = data;
    t_desc->next = NULL;
    t_desc->prev = NULL;
    t_desc->pcpu_tq = NULL;
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
struct flush_struct *__create_init_flush_struct
                                        (struct taskqueue_struct *tq_desc){
    struct flush_struct *f_desc = NULL;
    int index = 0;
    f_desc = (struct flush_struct *)malloc(sizeof(struct flush_struct));
    if(f_desc == NULL){
        //TODO error message for not allocating memory
        return NULL;
    }
    f_desc->task_id = (int *)malloc(tq_desc->count_pcpu_tq * sizeof(int));
    if(f_desc->task_id == NULL){
        //TODO error message for not allocating memory
        free(f_desc);
        return NULL;
    }
    for(index = 0; index < tq_desc->count_pcpu_tq; index++)
        *(f_desc->task_id + index) = 0;
    pthread_mutex_init(&(f_desc->flush_lock), NULL);
    pthread_cond_init(&(f_desc->flush_cond), NULL);
    f_desc->num_wake_prereq = 0;
    f_desc->next = NULL;
    return f_desc;
}
/*
 * free_flush_struct    frees the flush structure passed as an input arg
 * @f_desc              flush structure to be freed
 *
 */
void free_flush_struct(struct flush_struct *f_desc){
    free(f_desc->task_id);
    free(f_desc);
}
/*
 * add_to_flush_list    adds a flush structure to the flush list in the task
 *      queue structure passed as an arg. this function MUST be called after 
 *      locking the flush list for the taskqueue
 * @f_desc              flush structure to be added
 * @tq_desc             taskqueue where the flush struct is to be added
 */
void add_to_flush_list(struct flush_struct *f_desc,
                       struct taskqueue_struct *tq_desc){
    if(tq_desc->flushlist_tail == NULL){
        tq_desc->flushlist_tail = f_desc;
        tq_desc->flushlist_head = f_desc;
    }else{
        tq_desc->flushlist_tail->next = f_desc;
        tq_desc->flushlist_tail = f_desc;
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
void __remove_from_flush_list(struct flush_struct *f_desc,
                              struct taskqueue_struct *tq_desc){
    struct flush_struct *f_current = tq_desc->flushlist_head;
    struct flush_struct *f_prev = tq_desc->flushlist_head;
    while(f_current != NULL){
        if(f_current == f_desc)
            break;
        f_prev = f_current;
        f_current = f_current->next;
    }
    if(f_current == NULL){
        //TODO the flush struct is not found in the queue! error handling
        return;
    }
    if(f_current == tq_desc->flushlist_head)
        tq_desc->flushlist_head = tq_desc->flushlist_head->next;
    else
        f_prev = f_current->next;
    free_flush_struct(f_current);
    if(tq_desc->flushlist_head == NULL)
        tq_desc->flushlist_tail == NULL;
    return;
}
/*
 * __get_flush_dependencies 
 *                      find the tail tasks in all the per cpu taskqueues
 *      belonging to a taskqueue and add their ids to the flush struct. these 
 *      tasks need to be completed before a thread waiting on a call to 
 *      flush_taskqueue wakes. also, updates the count for total number of 
 *      dependencies for the waiting thread
 * @f_desc              pointer to the flush structure to be filled
 * @tq_desc             pointer to the taskqueue structure
 *
 */
void __get_flush_dependencies(struct flush_struct *f_desc, 
                          struct taskqueue_struct *tq_desc){
    int index = 0;
    struct percpu_taskqueue_struct *pc_tq = NULL;
    for(index = 0; index < tq_desc->count_pcpu_tq; index++){
        pc_tq = tq_desc->pcpu_tq + index;
        pthread_mutex_lock(&(pc_tq->lock));
            if(pc_tq->tlist_tail != NULL){
                *(f_desc->task_id + index) = pc_tq->tlist_tail->task_id;
                (f_desc->num_wake_prereq)++;
            }
        pthread_mutex_unlock(&(pc_tq->lock));
    }
}
/*
 * check_flush_queue    checks if a task_id is present in the flush list 
 *      associated with a task queue. if present, the task_id is set to 0,
 *      num_wake_prereq is decremented by one, and if num_wake_prereq becomes
 *      0, signal the thread waiting on corresponding flush_cond variable
 * @tq_desc             taskqueue structure where the flush list resides
 * @pc_tq_id            id of the percpu task queue to which the task belongs
 * @task_id             id of the task to be searched
 */
void check_flush_queue(struct taskqueue_struct *tq_desc, int pc_tq_id,
                       int task_id){
    struct flush_struct *f_desc_next = tq_desc->flushlist_head;
    struct flush_struct *f_desc = NULL;
    while((f_desc = f_desc_next) != NULL){
        if(*(f_desc->task_id + pc_tq_id) != task_id)
            break;
        pthread_mutex_lock(&(f_desc->flush_lock));
            f_desc->num_wake_prereq--;
            *(f_desc->task_id + pc_tq_id) = 0;
            f_desc_next = f_desc->next;
            if(f_desc->num_wake_prereq == 0)
                pthread_cond_signal(&(f_desc->flush_cond));
        pthread_mutex_unlock(&(f_desc->flush_lock));
    }
}
/*
 * round_robin_pctq     returns the per cpu taskqueue according to a round 
 *      robin scheme
 * @tq_desc             task queue to be investigated
 *
 * this function MUST be called in a synchronized manner in case when we have
 * multiple per cpu taskqueues per taskqueue
 * returns the per cpu taskqueue structure that contains least number of tasks
 */
struct percpu_taskqueue_struct *round_robin_pctq
                                    (struct taskqueue_struct * tq_desc){
    tq_desc->next_rr_pctq = (++tq_desc->next_rr_pctq)%(tq_desc->count_pcpu_tq);
    return tq_desc->pcpu_tq + tq_desc->next_rr_pctq;
}
/*
 * __select_pctq    selects a percpu taskqueue, according to the selection
 *      algorithm set for the taskqueue structure
 * @tq_desc         pointer to the taskqueue structure
 *
 * returns a pointer to the selected percpu taskqueue structure
 */
struct percpu_taskqueue_struct *__select_pctq
                                    (struct taskqueue_struct * tq_desc){
    struct percpu_taskqueue_struct *pc_tq = NULL;
    switch(tq_desc->pcpu_tq_selection_algo){
        case 0: 
            select_pctq = PCTQ_SEL_ALGO;
            break;
    }
    pthread_mutex_lock(&(tq_desc->tq_lock));
        pc_tq = select_pctq(tq_desc);
    pthread_mutex_unlock(&(tq_desc->tq_lock));
    return pc_tq;
}
/*
 * worker_thread        function executed by each worker thread. 
 *      each thread runs in the following loop 
 *      1. acquire the mutex in the per cpu taskqueue 
 *      2. check the number of tasks pending in this taskqueue
 *          2.1 sleep on the condition variable if there are no task to 
 *          execute else, 
 *          2.2 remove a task from the head of task linked list and 
 *          reduce the number of pending tasks in the taskqueue 
 *      3. release the mutex acquired earlier
 *      4. execute the task by calling the registered function
 *      5. free up the task structure.
 * @data                data for the thread to work upon, passed as a void 
 *      pointer
 */
void *worker_thread(void *data){
    struct percpu_taskqueue_struct *pc_tq = 
        (struct percpu_taskqueue_struct *)data;
    struct task_struct *t_desc = NULL;
    int task_id = 0;
    while(1){
        pthread_mutex_lock(&(pc_tq->lock));
            if(pc_tq->num_tasks == 0){
                pthread_cond_wait(&(pc_tq->more_task), &(pc_tq->lock));
            }
            t_desc = pc_tq->tlist_head;
            if(pc_tq->tlist_head == pc_tq->tlist_tail)
                pc_tq->tlist_tail = NULL; 
            pc_tq->tlist_head = pc_tq->tlist_head->next;
            if(pc_tq->tlist_head != NULL)
                pc_tq->tlist_head->prev = NULL;
            pc_tq->num_tasks--; 
        pthread_mutex_unlock(&(pc_tq->lock));
        //TODO We cannot return anything here. Seems right though.
        t_desc->fn(t_desc->data);
        //TODO this function should ideally be a separate thread
        task_id = t_desc->task_id;
        check_flush_queue(pc_tq->tq_desc, pc_tq->id, task_id);
        //TODO Test that it does not release data or pcpu_tq structures
        free(t_desc);
    }
}
/*
 * create_taskqueue     creates a taskqueue with the name tq_name. It creates
 *      'n' worker threads, one for each of the CPUs in the system. Threads are
 *      named after the taskqueue name passed as a parameter.
 * @tq_name             name for the taskqueue to be created
 *
 * returns a pointer to the taskqueue structure that describes the task queue
 */
struct taskqueue_struct *create_taskqueue(char *tq_name){
    int num_queues, index;
    struct percpu_taskqueue_struct *pc_tq = NULL;
    /* allocate taskqueue */
    struct taskqueue_struct *tq_desc = 
        (struct taskqueue_struct *)malloc(sizeof(struct taskqueue_struct));
    if(tq_desc == NULL){
        //TODO error handling
        return NULL;
    }
    /* assign the percpu taskqueue selection algorithm */
    tq_desc->pcpu_tq_selection_algo = 0;
    pthread_mutex_init(&(tq_desc->tq_lock), NULL);
    pthread_mutex_init(&(tq_desc->flushlist_lock), NULL);
    tq_desc->flushlist_head = NULL;
    tq_desc->flushlist_tail = NULL;
    tq_desc->next_rr_pctq = 0;
    num_queues = num_CPU();
    tq_desc->count_pcpu_tq = num_queues;
    /* allocate percpu taskqueues */
    tq_desc->pcpu_tq = (struct percpu_taskqueue_struct *)malloc(num_queues * 
                               sizeof(struct percpu_taskqueue_struct));
    if(tq_desc->pcpu_tq == NULL){
        //TODO error handling
        free(tq_desc);
        return NULL;
    }
    /* initialize percpu taskqueues */
    for(index = 0; index < num_queues; index++){
        pc_tq = (tq_desc->pcpu_tq + index);
        pthread_mutex_init(&(pc_tq->lock), NULL);
        pthread_cond_init(&(pc_tq->more_task), NULL);
        pc_tq->id = index;
        pc_tq->tlist_head = NULL;
        pc_tq->tlist_tail = NULL;
        pc_tq->num_tasks = 0;
        pc_tq->tq_desc = tq_desc; 
        pthread_create(&(pc_tq->worker), NULL, worker_thread, (void *)pc_tq);
    }
    return tq_desc;
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
struct taskqueue_struct *create_singlethread_taskqueue(char *tq_name){
    struct taskqueue_struct *tq_desc = 
        (struct taskqueue_struct *)malloc(sizeof(struct taskqueue_struct));
    if(tq_desc == NULL){
        //TODO error handling
        return NULL;
    }
    //TODO initialize the taskqueue_struct and create the threads
    return tq_desc;
}
/*
 * destroy_taskqueue    destroys a taskqueue
 * @tq_desc             pointer to the taskqueue_struct to be destroyed
 */
void destroy_taskqueue(struct taskqueue_struct *tq_desc){
}
/*
 * queue_task           queues a task in a task queue. Sets the pending field 
 *      in task struct to 1. The function checks if the task is already present
 *      in the task queue. If so, it returns immediately. Once the task has 
 *      been added to the task queue, the function wakes any worker thread 
 *      sleeping on the more_task wait queue in the local CPU's 
 *      percpu_taskqueue decriptor.
 * @tq_desc             pointer to the task queue where task is to be queued
 * @fn                  pointer to the function to be executed by the task
 * @data                data to be operated upon by the function
 *
 * returns 0, if task is added to the task queue
 *         1, if task was already present in the task queue
 *        -1, in case of any errors
 */
int queue_task(struct taskqueue_struct *tq_desc, void(* fn)(void *),        \
               void *data){
    //TODO How to make sure if a task is already present in the tq ?
    struct percpu_taskqueue_struct *pc_tq = __select_pctq(tq_desc);
    struct task_struct *t_desc = __create_task_struct(fn, data);
    if(t_desc == NULL){
        //TODO log error message
        return -1;
    }
    t_desc->pcpu_tq = pc_tq;
    pthread_mutex_lock(&(pc_tq->lock));
    if(pc_tq->tlist_tail != NULL)
        t_desc->task_id = (pc_tq->tlist_tail->task_id) + 1;
    else
        t_desc->task_id = 1;
    if(t_desc->task_id < 1)
        t_desc->task_id = 1;
    if(pc_tq->tlist_tail != NULL){
        pc_tq->tlist_tail->next = t_desc;
        t_desc->prev = pc_tq->tlist_tail;
        pc_tq->tlist_tail = t_desc;
    }else{
        pc_tq->tlist_head = t_desc;
        pc_tq->tlist_tail = t_desc;
    }
    pc_tq->num_tasks++;
    if(pc_tq->num_tasks == 1)
        pthread_cond_signal(&(pc_tq->more_task));
    pthread_mutex_unlock(&(pc_tq->lock));
    return 0;
}
/*
 * queue_delayed_task   this function is similar to queue_task except that it 
 *      adds a task in the percpu task queue after a certain delay specified
 *      as the third parameter to the function.
 * @tq_desc             pointer to the task queue where task is to be queued
 * @fn                  pointer to the function to be executed by the task
 * @data                data to be operated upon by the function
 * @delay               delay in <> after which the task is to be added to the
 *      percpu task queue descriptor.
 *
 * returns 0, if task is scheduled to be added the percpu task queue
 *         1, if task was already present in the task queue
 *        -1, in case of any errors
 */
int queue_delayed_task(struct taskqueue_struct * tq_desc, void(* fn)(void *),
                       void *data, unsigned long delay){
    return 0;
}
/*
 * cancel_delayed_task  cancels a task that was scheduled to be added to a per
 *      cpu task queue after a delay. If the task has already been added to the
 *      per cpu task queue, the function returns.
 * @t_desc              descriptor for the task to be cancelled
 *
 * returns 0, upon successfully cancelling a task
 *         1, if the task has already been added to the percpu task queue
 *         -1, in case of any error
 */
int cancel_delayed_task(struct task_struct *t_desc){
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
int flush_taskqueue(struct taskqueue_struct *tq_desc){
    pthread_mutex_lock(&(tq_desc->flushlist_lock));
        int index = 0;
        struct flush_struct *f_desc = NULL;
        f_desc = __create_init_flush_struct(tq_desc);
        if(f_desc == NULL)
            return -1;
        pthread_mutex_lock(&(f_desc->flush_lock));
            __get_flush_dependencies(f_desc, tq_desc);
            if(f_desc->num_wake_prereq == 0){
                pthread_mutex_unlock(&(f_desc->flush_lock));
                free_flush_struct(f_desc);
                pthread_mutex_unlock(&(tq_desc->flushlist_lock));
                return 0;
            }else{
                add_to_flush_list(f_desc, tq_desc);
                pthread_mutex_unlock(&(tq_desc->flushlist_lock));
                pthread_cond_wait(&(f_desc->flush_cond), 
                                  &(f_desc->flush_lock));
            }
            pthread_mutex_lock(&(tq_desc->flushlist_lock));
                __remove_from_flush_list(f_desc, tq_desc);
            pthread_mutex_unlock(&(tq_desc->flushlist_lock));
            return 0;
}
