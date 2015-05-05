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
struct flush_s{
    /* lock to protect the count variable and to be used with cond variable */
    pthread_mutex_t flush_lock;
    /* condition variable for the calling thread to sleep on */
    pthread_cond_t flush_cond;
    /* counter representing number of tasks to wait upon */
    int num_wake_prereq;
    /* array of task sequence number from all the sub taskqueues */
    int *task_id;
    /* pointer to the next flush struct */
    struct flush_s *next;
};
typedef struct flush_s flush_t;
/* describes the task queue */
struct taskqueue_s{
    /* number of sub taskqueues in this taskqueue */
    int tq_num_stq;
    /* mutex to protect write access to tq_usable_stq */
    pthread_mutex_t tq_lock_usable_stq;
    /* number of usable sub taskqueues in this taskqueue */
    int tq_usable_stq;
    /* array of sub taskqueues, each handled by a separate thread */
    struct sub_taskqueue_s *s_tq; 
    /* algorithm to be used to select a sub taskqueue for adding a task */
    int tq_stq_sel_algo;
    /* mutex to be used for selecting a sub taskqueue while adding a task */
    pthread_mutex_t tq_lock;
    /* round robin sub taskqueue selection counter */
    int tq_next_rr_stq;
    /* lock for adding/removing flush structures from flush linked list */
    pthread_mutex_t flushlist_lock;
    /* head for the linked list of flush structures */
    flush_t *flushlist_head;
    /* tail for the linked list of flush structures */
    flush_t *flushlist_tail;
    /* identifier for the task queue */
    char *id;
};
typedef struct taskqueue_s taskqueue_t;
/* describes the task in the queue */
struct task_s{
    /* identifier for the task(unique within each sub taskqueue) */
    int task_id;
    /* pointer to the next task */
    struct task_s *next;
    /* pointer to the pending function */
    void(*fn)(void *);
    /* pointer to the data to be passed to the pending function */
    void *data;
    /* pointer to the sub_taskqueue where this struct is queued */
    struct sub_taskqueue_s *s_tq;
    /* software timer to delay addition of task to a task queue */
    //timer;
};
typedef struct task_s task_t;
/* describes the sub_taskqueue */
struct sub_taskqueue_s{
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
    task_t *tlist_head;
    /* tail of the linked list for tasks in the queue */
    task_t *tlist_tail;
    /* number of tasks queued, -1 signifies sub taskqueue locked */
    int num_tasks;
    /* pointer to the taskqueue this sub taskqueue struct belongs to */
    taskqueue_t *tq_desc;
};
typedef struct sub_taskqueue_s sub_taskqueue_t;
/* funtions used to create a taskqueue */
taskqueue_t *create_taskqueue(char *);
taskqueue_t *create_singlethread_taskqueue(char *);
/* functions used to destroy a taskqueue */
void destroy_taskqueue(taskqueue_t *);
/* functions to queue a task */
int queue_task(taskqueue_t *, void(*)(void *), void *);
int queue_delayed_task(taskqueue_t *, void(*)(void *), void *, unsigned long);
/* function to cancel a task scheduled for delayed queuing */
int cancel_delayed_task(task_t *);
/* function to flush a task queue */
int flush_taskqueue(taskqueue_t *);
