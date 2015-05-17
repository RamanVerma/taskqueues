# taskqueues
Taskqueues is a static library that emulates workqueue infrastructure of the 
Linux Kernel in User space. It is based on the PThreads library.

##Project Statement

A. Provide a simple API to the user space C programs for queueing multiple 
tasks in queues, one each for a configurable number of threads.

B. Take away the complexity of defining/maintaining structures required to 
queue tasks and to synchronize among the threads operating on these queues. 
Thereby allowing programmer to focus on business logic while using a fairly
advanced infrastructure to parallelize/synchronize work.

C. Provide a single API call for each of the following jobs.

    1. define taskqueues with a configurable number of threads.

    2. queue tasks to a queue structure, one belonging to each thread.

    3. synchronization call to wait for all the currently queued tasks to finish

    4. destroy the whole taskqueue structure gracefully.

D. This library can enhance the performance for multi threaded programs by         
avoiding multiple context switches. If we randomly create a thread for each        
task, or create a huge number of threads, there will certainly be un necessary  
context switching in the kernel. Rather, we create same number of threads as       
the number of CPU cores(default) and make them work on queues holding tasks.       
Hence, the overhead of thread creation, deletion and context switch goes away!! 
Also, if the tasks are more IO Intensive, we can create taskqueues with a large
number of threads and if the tasks are CPU Intensive, we can create the default
taskqueues.

Please visit the project wiki to checkout the Design and API Documentation.
