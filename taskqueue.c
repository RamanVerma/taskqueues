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
 * run_taskqueue        runs the percpu taskqueue
 * @tasks               array of tasks to be executed
 */
void run_taskqueue(void *tasks){
    /* return if the tasks array is NULL */
    if(tasks == NULL)
        return;
    /* execute all the tasks */
    struct task_struct *t_desc = (struct task_struct *)tasks;
    while(t_desc != NULL){
        //TODO execute the task
        t_desc = t_desc->next;
    }
    return;
}
/*
 * get_task_dec_count   gets the task structure pointed to by the head of the 
 *      doubly linked list of tasks in the per cpu task queue. the function
 *      moves the head and tail pointers of the doubly linked list accordingly
 *      and decrements the num_tasks counter.
 * @pc_tq               pointer to the per cpu taskqueue
 *
 * returns pointer to the task structure at the head of linked list containing
 *      tasks
 */
struct task_struct *get_task_dec_count(struct percpu_taskqueue_struct *pc_tq){
    struct task_struct *t_desc = NULL;
    return t_desc;
 }
/*
 * worker_thread        function executed by a worker thread
 * @data                data for the thread to work upon, passed as a void 
 *      pointer
 */
void *worker_thread(void *data){
    struct percpu_taskqueue_struct *pc_tq = 
        (struct percpu_taskqueue_struct *)data;
    struct task_struct *task = NULL;
    while(1){
        pthread_mutex_lock(&(pc_tq->lock));
            if(pc_tq->num_tasks == 0){
                pthread_cond_wait(&(pc_tq->more_task), &(pc_tq->lock));
            }
            task = get_task_dec_count(pc_tq);
//        run_taskqueue(data);
        pthread_mutex_unlock(&(pc_tq->lock));
        //execute_task(task);
        //free(task);
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
    /* allocate percpu taskqueues */
    num_queues = num_CPU();
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
 * @t_desc              pointer to the task struct to be queued
 *
 * returns 0, if task is added to the task queue
 *         1, if task was already present in the task queue
 *        -1, in case of any errors
 */
int queue_task(struct taskqueue_struct *tq_desc, struct task_struct *t_desc){
    return 0;
}
/*
 * queue_delayed_task   this function is similar to queue_task except that it 
 *      adds a task in the percpu task queue after a certain delay specified
 *      as the third parameter to the function.
 * @tq_desc             pointer to the task queue where task is to be queued
 * @t_desc              pointer to the task struct to be queued
 * @delay               delay in <> after which the task is to be added to the
 *      percpu task queue descriptor.
 *
 * returns 0, if task is scheduled to be added the percpu task queue
 *         1, if task was already present in the task queue
 *        -1, in case of any errors
 */
int queue_delayed_task(struct taskqueue_struct * tq_desc, 
                       struct task_struct * t_desc, 
                       unsigned long delay){
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

/*
 * main                 main method
 */
int main(){
}
