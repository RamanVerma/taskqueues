/*
 * =====================================================================================
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
 * =====================================================================================
 */
#include<stdio.h>
#include"../taskqueue.c"

/* simple task for the task queues */
void simplePrint(void *data){
    char *msg = (char *)data;
    printf("Message String: %s\n", msg);
}

void test1(){
    /* Get the number of CPUs/cores in the system */
    printf("__func__");
    printf("The number of CPU in this system: %d", num_CPU());
    return;
}

void test2(){
    /* Create a taskqueue */
    printf("__func__");
    struct taskqueue_struct *tq_desc = NULL;
    char *name = "Queue1";
    tq_desc = create_taskqueue(name);
    if(tq_desc == NULL){
        printf("Could not create taskqueue\n");
        return;
    }
    char text[] = {"Hello"};
    int x = 0;
    for(x=0; x<5; x++)
        queue_task(tq_desc, simplePrint, (void *)text); 
    return;
}

int main(){
    test1();
    test2();
    return 0;
}
