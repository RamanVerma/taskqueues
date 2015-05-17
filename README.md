# taskqueues
Taskqueues is a static library that emulates workqueue infrastructure of the 
Linux Kernel in User space. It is based on the PThreads library.

++AIM++

A. Provide a simple API to the user space C programs for queueing multiple 
tasks in thread specific queues, while having a configurable number of threads.

B. Take away the complexity of defining/maintaining structures required to 
queue tasks and to synchronize among the threads operating on these queues.

C. Provide a single API call for each of the following jobs.
1. define taskqueues with a configurable number of threads.
2. queue tasks to a queue structure one belonging to each thread.
3. synchronization call to wait for all the currently queued tasks to finish.
4. destroy the whole taskqueue structure gracefully.

Please visit the project wiki to checkout the Design and API Documentation.
