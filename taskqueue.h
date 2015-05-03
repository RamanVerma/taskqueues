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

/* information for a thread waiting for flush_taskqueue operation */
struct flush_struct{
    /* lock to protect the count variable and to be used with cond variable */
    pthread_mutex_t flush_lock;
    /* condition variable for the calling thread to sleep on */
    pthread_cond_t flush_cond;
    /* counter representing number of tasks to wait upon */
    int num_wake_prereq;
    /* array of task sequence number from all the sub taskqueues */
    int *task_id;
    /* pointer to the next flush struct */
    struct flush_struct *next;
};

/* describes the task queue */
struct taskqueue_struct{
    /* number of sub taskqueues in this taskqueue */
    int count_s_tq;
    /* number of usable sub taskqueues in this taskqueue */
    int tq_usable_stq;
    /* array of sub taskqueues, each handled by a separate thread */
    struct sub_taskqueue_struct *s_tq; 
    /* algorithm to be used to select a sub taskqueue for adding a task */
    int s_tq_selection_algo;
    /* mutex to be used for selecting a sub taskqueue while adding a task */
    pthread_mutex_t tq_lock;
    /* round robin sub taskqueue selection counter */
    int next_rr_s_tq;
    /* lock for adding/removing flush structures from flush linked list */
    pthread_mutex_t flushlist_lock;
    /* head for the linked list of flush structures */
    struct flush_struct *flushlist_head;
    /* tail for the linked list of flush structures */
    struct flush_struct *flushlist_tail;
    /* identifier for the task queue */
    char *id;
};
/* describes the task in the queue */
struct task_struct{
    /* identifier for the task(unique within each sub taskqueue) */
    int task_id;
    /* pointer to the next task */
    struct task_struct *next;
    /* pointer to the pending function */
    void(*fn)(void *);
    /* pointer to the data to be passed to the pending function */
    void *data;
    /* pointer to the sub_taskqueue where this struct is queued */
    struct sub_taskqueue_struct *s_tq;
    /* software timer to delay addition of task to a task queue */
    //timer;
};
/* describes the sub_taskqueue */
struct sub_taskqueue_struct{
    /* id of the sub task queue */
    int stq_id;
    /* mutex to synchronize worker and destroy/SIGKILL access to task list */
    pthread_mutex_t worker_lock;
    /* condition variable for thread waiting for work */
    pthread_cond_t more_task;
    /* thread that will execute tasks queued for this sub taskqueue */
    pthread_t worker;
    /* mutex to lock the taskqueue */
    pthread_mutex_t lock;
    /* head of the linked list for tasks in the queue */
    struct task_struct *tlist_head;
    /* tail of the linked list for tasks in the queue */
    struct task_struct *tlist_tail;
    /* number of tasks queued, -1 signifies sub taskqueue locked */
    int num_tasks;
    /* pointer to the taskqueue this sub taskqueue struct belongs to */
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
int flush_taskqueue(struct taskqueue_struct *);
