/*
 * ============================================================================
 *
 *       Filename:  taskqueue.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/04/2015 11:49:33 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Verma, Raman
 *   Organization:  
 *
 * ============================================================================
 */
#include<pthread.h>

/* describes the task queue */
struct taskqueue_struct{
    /* number of percpu task queues in this taskqueue */
    int count_pcpu_tq;
    /* array of task queues, one for each CPU */
    struct percpu_taskqueue_struct *pcpu_tq; 
    /* algorithm to be used to select a per cpu taskqueue for adding a task */
    int pcpu_tq_selection_algo;
    /* mutex to be used for selecting a percpu taskqueue */
    pthread_mutex_t tq_lock;
    /* round robin percpu taskqueue selection counter */
    int next_rr_pctq;
    /* identifier for the task queue */
    char *id;
};
/* describes the task in the queue */
struct task_struct{
    /* pointer to the next task */
    struct task_struct *next;
    /* pointer to the previous task */
    struct task_struct *prev;
    /* pointer to the pending function */
    void(*fn)(void *);
    /* pointer to the data to be passed to the pending function */
    void *data;
    /* pointer to the parent percpu_taskqueue */
    struct percpu_taskqueue_struct *pcpu_tq;
    /* software timer to delay addition of tak to a task queue */
    //timer;
};
/* describes the percpu_taskqueue */
struct percpu_taskqueue_struct{
    /* mutex to lock the taskqueue */
    pthread_mutex_t lock;
    /* condition variable for thread waiting for work */
    pthread_cond_t more_task;
    /* thread that will execute tasks queued for this CPU */
    pthread_t worker;
    /* head of the linked list for tasks in the queue */
    struct task_struct *tlist_head;
    /* tail of the linked list for tasks in the queue */
    struct task_struct *tlist_tail;
    /* number of taks queued */
    int num_tasks;
    /* pointer to the taskqueue this percpu structure belongs to */
    struct taskqueue_struct *tq_desc;
};
/* funtions used to create a taskqueue */
struct taskqueue_struct *create_taskqueue(char *);
struct taskqueue_struct *create_singlethread_taskqueue(char *);
/* functions used to destroy a taskqueue */
void destroy_taskqueue(struct taskqueue_struct *);
/* functions to queue a task */
int queue_task(struct taskqueue_struct *, void(*)(void *), void *);
int queue_delayed_task(struct taskqueue_struct *, void(*)(void *), void *, 
                       unsigned long);
/* function to cancel a task scheduled for delayed queuing */
int cancel_delayed_task(struct task_struct *);
/* function to flush a task queue */
void flush_taskqueue(struct taskqueue_struct *);
