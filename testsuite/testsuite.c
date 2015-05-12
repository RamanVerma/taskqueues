/*
 * ============================================================================
 *
 *       Filename:  testsuite.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/15/2015 05:24:04 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Verma, Raman
 *   Organization:  
 *
 * ============================================================================
 */
#include<stdio.h>
#include"../taskqueue.c"

/* simple task for the task queues */
void simpleprint(void *data){
    char *msg = (char *)data;
    printf("Message String: %s\n", msg);
}

/* sleep for a specified number of seconds */
void sleeper(void *data){
    int t = (int)data;
    printf("Sleep for %d sec\n", t);
    sleep(t);
    printf("Done\n");
}

void test1(){
    /* Get the number of CPUs/cores in the system */
    printf(__func__);
    printf("\nnum of CPU in this machine:%d\n", __num_CPU());
    return;
}

void test2(){
    /* Create a taskqueue */
    printf(__func__);
    taskqueue_t *tq_desc = NULL;
    int i = 0, n = 4;
    char buf[16] = {'\0'};
    char *name = "Queue1";
    tq_desc = create_custom_taskqueue(name, n, 0);
    if(tq_desc == NULL){
        printf("\nCould not create taskqueue\n");
        return;
    }
    printf("\ntaskqueue name:%s\n", tq_desc->tq_id);
    for(i=0; i < n; i++){
        pthread_getname_np((tq_desc->tq_stq + i)->s_worker, buf, sizeof(buf));
        printf("thread name: %s\n", buf); 
    }
    return;
}

void test3(){
    /* Queue tasks */
    printf(__func__);
    taskqueue_t *tq_desc = NULL;
    char *name = "Queue1";
    tq_desc = create_taskqueue(name);
    if(tq_desc == NULL){
        printf("Could not create taskqueue\n");
        return;
    }
    char text[] = {"Hello"};
    int x = 0;
    for(x=0; x<5; x++)
        queue_task(tq_desc, simpleprint, (void *)text); 
    return;
}

void test4(){
    /* test flush_taskqueue */
    printf(__func__);
    taskqueue_t *tq_desc = NULL;
    char *name = "Queue1";
    tq_desc = create_taskqueue(name);
    if(tq_desc == NULL){
        printf("Could not create taskqueue\n");
        return;
    }
    int time = 2;
    int x = 0;
    for(x = 0; x < 15; x++)
        queue_task(tq_desc, sleeper, (void *)time);
    flush_taskqueue(tq_desc);
    return;
}

void test5(){
    /* Create a taskqueue */
    printf(__func__);
    taskqueue_t *tq_desc = NULL;
    int i = 0, n = 2;
    char buf[16] = {'\0'};
    char *name = "Queue1";
    tq_desc = create_custom_taskqueue(name, n, 0);
    if(tq_desc == NULL){
        printf("\nCould not create taskqueue\n");
        return;
    }
    printf("\ntaskqueue name:%s\n", tq_desc->tq_id);
    for(i=0; i < n; i++){
        pthread_getname_np((tq_desc->tq_stq + i)->s_worker, buf, sizeof(buf));
        printf("thread name: %s\n", buf); 
    }
    destroy_taskqueue(tq_desc);
    return;
}

int main(){
    test1();
//    test2();
//    test3();
//    test4();
    test5();
    return 0;
}
