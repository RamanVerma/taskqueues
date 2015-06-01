/*
 * ============================================================================
 *
 *       Filename:  testsuite.c
 *
 *    Description:  Basic tests to sanity test the taskqueue library 
 *
 *        Version:  1.0
 *        Created:  03/15/2015 05:24:04 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Verma, Raman
 *   Organization:  
 *
 *
 * ============================================================================
 */
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include"taskqueue.h"

#define MAX_ARR_SZ 10
/* global pointers to arrays */
int **a, **b, **c;
int row_a, col_a, row_b, col_b;

/* print an array */
void printarray(int **arr, int rows, int cols) {
	int index, index2;
	printf("++++++++++++++++++++++++++++++++++++\n");
	printf("Rows: %d, Cols: %d\n", rows, cols);
	for (index = 0; index < rows; index++) {
		for (index2 = 0; index2 < cols; index2++) {
			printf("%d\t", *((*arr + index) + index2));
		}
		printf("\n");
	}
	printf("++++++++++++++++++++++++++++++++++++\n");
}

/* multiply a row and a column of two matrices */
/* TODO Test this function */
void mul(int ra, int cb, int dim2_a) {
    int index = 0, elem_a, elem_b, elem_c;
    for (index = 0; index < dim2_a; index++) {
        elem_a = *(a[ra] + index);
	elem_b = *(b[index] + cb);
	elem_c += elem_a * elem_b;
    }
    *(c[ra] + cb) = elem_c;
}

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
    int index = 0;
    srand(time(NULL));
    /* Allocate and Initialize the first array */
    row_a = rand() % MAX_ARR_SZ;
    col_a = rand() % MAX_ARR_SZ;
    if (row_a == 0)
    	row_a = 5;
    if (col_a == 0)
    	col_a = 5;
    a = (int **)malloc(row_a * sizeof(int *));
    for (index = 0;index < row_a; index++) {
    	a[index] = (int *)malloc(col_a * sizeof(int));
	//memset(a[index], 1, sizeof(a[index]));
	memset(a[index], 0, col_a * sizeof(int));
    }
    printarray(a, row_a, col_a);
    /* Allocate and Initialize the second array */
    row_b = col_a;
    col_b = rand() % MAX_ARR_SZ;
    if (col_b == 0)
    	col_b = 5;
    b = (int **)malloc(row_b * sizeof(int *));
    for (index = 0;index < row_b; index++) {
    	b[index] = (int *)malloc(col_b * sizeof(int));
	memset(b[index], 0, col_b * sizeof(int));
    }
    printarray(b, row_b, col_b);
    /* Allocate and Initialize the resultant array */
    c = (int **)malloc(row_a * sizeof(int *));
    for (index = 0;index < row_a; index++) {
    	c[index] = (int *)malloc(col_b * sizeof(int));
	memset(c[index], 0, col_b * sizeof(int));
    }
    /* Run tests */
    test1();
//    test2();
//    test3();
//    test4();
//    test5();
    /*TODO Write test case that does matrix multiplication */
    return 0;
}
