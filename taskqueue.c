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
    return num;
 }
/*
 * create_task_struct   creates a task struct from the function and data 
 *      passed as input.
 * @fn                  pointer to the function to be wrapped in task struct
 * @data                pointer to the data to be operated upon
 *
 * returns pointer to the task struct, or NULL in case of any error
 */
struct task_struct *create_task_struct(void(*fn)(void *), void *data){
    struct task_struct * t_desc =
                    (struct task_struct *)malloc(sizeof(struct task_struct));
    if(t_desc == NULL){
        //TODO error message handling
        return NULL;
    }
    t_desc->fn = fn;
    t_desc->data = data;
    t_desc->next = NULL;
    t_desc->prev = NULL;
    t_desc->pcpu_tq = NULL;
    return t_desc;
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
        //TODO Test that it does not release data or pcpu_tq structures
        free(t_desc);
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
    tq_desc->next_rr_pctq = (tq_desc->next_rr_pctq++)%(tq_desc->count_pcpu_tq);
    return tq_desc->pcpu_tq + tq_desc->next_rr_pctq;
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
    struct task_struct *t_desc = NULL;
    struct percpu_taskqueue_struct *pc_tq = NULL;
    switch(tq_desc->pcpu_tq_selection_algo){
        case 0: 
            select_pctq = PCTQ_SEL_ALGO;
            break;
    }
    pthread_mutex_lock(&(tq_desc->tq_lock));
        pc_tq = select_pctq(tq_desc);
    pthread_mutex_unlock(&(tq_desc->tq_lock));
    t_desc = create_task_struct(fn, data);
    if(t_desc == NULL){
        //TODO log error message
        return -1;
    }
    t_desc->pcpu_tq = pc_tq;
    pthread_mutex_lock(&(pc_tq->lock));
    t_desc->next = NULL;
    t_desc->prev = NULL;
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
 */
void flush_taskqueue(struct taskqueue_struct *tq_desc){
}
