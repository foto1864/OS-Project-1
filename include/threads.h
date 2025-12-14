/*  threads.h 
    This file contains declarations of the struct and the functions related to the 
    thread-based message handling. Each process spawns two threads, one for reading
    message and one for sending messages. The threads operate on the shared memory
    structures that are defined in utils.h 
*/

#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>
#include <signal.h>

#include "utils.h"

/*  The thread_args_t struct is used in order to bundle all the arguments needed 
    for the threads together, as the pthreads fucntion accepts only once void* argument.
    shared_mem: pointer to the global shared memory segment
    dialog: pointer to the dialog this process participates in
    dialog_id: id of the dialog
    my_slot: the slot index that is assigned to this process
    smd_id: shared memory id (it is used for cleanup)
    terminate_flag: signal-sage flag used to request the termination of the thread. More on that below.
*/
typedef struct {
    shared_memory_t *shared_mem;
    dialog_t *dialog;
    int dialog_id;
    int my_slot;
    int shm_id;
    volatile sig_atomic_t *terminate_flag;
} thread_args_t;

/*  Waits for new messages addressed to this slot, prints/processes
    these messages until termination is requested
*/
void* reader_thread(void* arg);

/*  Reads user input and sends messages to teh dialog until termination is requested
*/
void* writer_thread(void* arg);

/*  Sends a message to the given dialog. It returns 0 on success or a negative
    value if the message cannot be sent.
*/
int message_send(dialog_t *dialog, int my_slot, const char *text);

/*  Receives the next unread message for the given slot. It returns 0 if 
    a message is received or a negative value if no message is available.
*/
int message_receive(dialog_t *dialog, int my_slot);

#endif

/*  Comment about the usage of "volatile sig_atomic_t *terminate_flag":

    The  terminate_flag is used as a lightweight termination signal shared between
    the reader and writer threads. The reason it is declared as sig_atomic_t is because:
    1. sig_atomic_t is the only integer type guaranteed to be read and written
    atomically, even in the presence of signals.
    2. This prevents corrupted reads/writes if the flag is modified asynchronously.

    The reason it is declared as volatile is because:
    1. The value may be changed by another thread (or signal handler),
    outside the current execution flow.
    2. Without volatile, the compiler is allowed to cache the value in a register
    and never re-read it inside the loop, causing infinite loops.
    3. volatile forces the compiler to reload the value from memory on each access.

    A pointer is used so that multiple threads can observe and modify
    the same shared termination flag.

    Example:
    
    // Thread A
    while (1) {
        if (terminate_flag) break;   // must observe changes made by another thread
    }
    // Thread B
    terminate_flag = 1;              // request termination

    - sig_atomic_t guarantees the write is atomic.
    - volatile prevents the compiler from caching terminate_flag in a register,
    ensuring Thread A re-reads it from memory on every loop iteration.
 */
