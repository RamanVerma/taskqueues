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
 * ============================================================================
 */
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include"taskqueue.h"
#include<pthread.h>

#define MAX_ARR_SZ 10
#define DELIM_LEN 80
/* global pointers to arrays */
int **a, **b, **c;
int row_a, col_a, row_b, col_b;

pthread_mutex_t global_lock;

/*
 *alloc_init_2darray	allocates and initializes an array
 *@rows			number of rows in the array
 *@cols			number of columns in the array
 *
 *returns pointer to the 2 dimsensional array, -ENOMEM in case of error
 */
int **
alloc_init_2darray(int rows, int cols)
{
    int index = 0;
    int **arr = (int **)malloc(rows * sizeof(int *));
    if (arr == NULL)
    	return NULL;
    for (index = 0; index < rows; index++) {
    	*(arr + index) = (int *)malloc(cols * sizeof(int));
	if (*(arr + index) == NULL)
	    return NULL;
	memset(*(arr + index), 0, cols * sizeof(int));
    }
    return arr;
}

/*
 * populate_array	populates the array with a simple formula. every 
 * 	element holds a value equal to the index number of the element if the
 * 	array was one dimensional
 * @arr			array to be populated
 * @nrows		number of rows in the array
 * @ncols		number of columns in the array
 */
void
populate_array(int **arr, int nrows, int ncols)
{
    int x = 0, y = 0;
    for(x = 0; x < nrows; x++) {
    	for(y = 0; y < ncols; y++) {
	    *(*(arr + x) + y) = (x * ncols) + y;
	}
    }
}

/* 
 *printarray		prints a 2 dimensional array
 *@arr			pointer to pointer to array type
 *@rows			number of rows in the array
 *@cols			number of columns in the array
 */
void
printarray(int **arr, int rows, int cols)
{
	int index, index2;
	printf("++++++++++++++++++++++++++++++++++++\n");
	printf("Rows: %d, Cols: %d\n", rows, cols);
	for (index = 0; index < rows; index++) {
		for (index2 = 0; index2 < cols; index2++) {
			printf("%d\t", *(*(arr + index) + index2));
		}
		printf("\n");
	}
	printf("++++++++++++++++++++++++++++++++++++\n");
}

/*
 * basic_matrix_mul_op	basic operation of multiplying a row of one array with
 *	the column of another array
 * @ra			row of the first array
 * @cb			column of the second array
 * @dim2_a		number of columns in the first array
 *
 * stores the result in a third array
 */
void
basic_matrix_mul_op(int ra, int cb, int dim2_a)
{
    int index = 0, elem_a, elem_b, elem_c;
    for (index = 0; index < dim2_a; index++) {
        elem_a = *(*(a + ra) + index);
	elem_b = *(*(b + index) + cb);
	elem_c += elem_a * elem_b;
    }
    *(*(c + ra) + cb) = elem_c;
}

/*
 * simpleprint		prints a message passed on by the caller
 * @data		pointer to string to be printed
 */
void
simpleprint(void *data)
{
    char *msg = (char *)data;
    printf("Message String: %s\n", msg);
}

/*
 * sleeper		sleep for a specified number of seconds 
 * @data		pointer to int that specifies the number of seconds to
 * 	sleep
 */
void
sleeper(void *data)
{
    int t = *(int *)data;
    printf("Sleep for %d sec\n", t);
    sleep(t);
    printf("Done\n");
}

/*
 * inc_counter		increments a counter protecting it with a global lock
 * @counter_p		pointer to the counter to be incremented
 */
void
inc_counter(void *counter_p)
{
	pthread_mutex_lock(&global_lock);
	*(int *)counter_p = (*(int *)counter_p)++;
	pthread_mutex_unlock(&global_lock);
}

/*
 * print_delimiter	prints a general purpose custom delimiter
 */
void
print_delimiter()
{
    int x = 0;
    for(x = 0; x < DELIM_LEN; x++) {
    	printf("-");
    }
    printf("\n");
}

/*
 * test_num_cpu_cores		checks the number of cpu cores in the system
 */
void
test_num_cpu_cores()
{
    printf(__func__);
    printf("\nnum of CPU in this machine:%d\n", __num_CPU());
    return;
}

/*
 * test_create_taskqueue	test to create a taskqueue
 */
void
test_create_taskqueue()
{
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

/*
 * test_queue_task		tests the queue functionality of taskqueues
 * 	queues a add task 5 times. expects the counter to be at value 5 after
 *	flush is returned
 * returns 0 on success and < 0 on error
 */
int
test_queue_task()
{
    printf(__func__);
    int *counter;
    taskqueue_t *tq_desc = NULL;
    char *name = "Queue1";
    tq_desc = create_taskqueue(name);
    if(tq_desc == NULL){
        printf("Could not create taskqueue\n");
        return -1;
    }
    int x = 0;
    *counter = 0;
    for(x=0; x<5; x++)
        queue_task(tq_desc, inc_counter, (void *)counter); 
    flush_taskqueue(tq_desc);
    if (*counter == ++x)
    	return 0;
    else
    	return -2;
}

/*
 * test_flush_taskqueue		tests the flush functionality of taskqueues
 */
int
test_flush_taskqueue()
{
    printf(__func__);
    taskqueue_t *tq_desc = NULL;
    char *name = "Queue1";
    tq_desc = create_taskqueue(name);
    if(tq_desc == NULL){
        printf("Could not create taskqueue\n");
        return -1;
    }
    int time = 2;
    int x = 0;
    for(x = 0; x < 15; x++)
        queue_task(tq_desc, sleeper, (void *)&time);
    flush_taskqueue(tq_desc);
    return 0;
}

/*
 * test_destroy_tq		print the taskqueue and worker threads' names
 * returns 0 on success and -1 on error
 */
int
test_destroy_tq()
{
    printf(__func__);
    taskqueue_t *tq_desc = NULL;
    int i = 0, n = 2;
    char buf[16] = {'\0'};
    char *name = "Queue1";
    tq_desc = create_custom_taskqueue(name, n, 0);
    if(tq_desc == NULL){
        printf("\nCould not create taskqueue\n");
        return -1;
    }
    printf("\ntaskqueue name:%s\n", tq_desc->tq_id);
    for(i=0; i < n; i++){
        pthread_getname_np((tq_desc->tq_stq + i)->s_worker, buf, sizeof(buf));
        printf("thread name: %s\n", buf); 
    }
    destroy_taskqueue(tq_desc);
    return 0;
}
/*
 * main function
 */
int
main()
{
    int index = 0;
    srand(time(NULL));
    /* Allocate and Initialize the first array */
    row_a = rand() % MAX_ARR_SZ;
    col_a = rand() % MAX_ARR_SZ;
    row_a = (row_a == 0) ? 5: row_a;
    col_a = (col_a == 0) ? 5: col_a;
    a = alloc_init_2darray(row_a, col_a);
    populate_array(a, row_a, col_a);
    printarray(a, row_a, col_a);
    /* Allocate and Initialize the second array */
    row_b = col_a;
    col_b = rand() % MAX_ARR_SZ;
    col_b = (col_b == 0) ? 5: col_b;
    b = alloc_init_2darray(row_b, col_b);
    populate_array(b, row_b, col_b);
    printarray(b, row_b, col_b);
    /* Allocate and Initialize the resultant array */
    c = alloc_init_2darray(row_a, col_b);
    pthread_mutex_init(&global_lock, NULL);
    basic_matrix_mul_op(0, 0, col_a);
    printarray(c, row_a, col_b);

    /* Run tests */
    print_delimiter();
    test_num_cpu_cores();
    print_delimiter();
//    test_create_taskqueue();
    print_delimiter();
//    test_queue_task();
    print_delimiter();
//    test_flush_taskqueue();
    print_delimiter();
//    test_destroy_tq();
    print_delimiter();
    /*TODO Write test case that does matrix multiplication */
    pthread_mutex_destroy(&global_lock);
    return 0;
}
