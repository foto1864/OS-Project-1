#ifndef THREADS_H
#define THREADS_H

/*  threads.h 
    This file contains declarations of the struct and the functions related to the 
    thread-based message handling. Each process spawns two threads, one for reading
    message and one for sending messages. The threads operate on the shared memory
    structures that are defined in utils.h 
*/

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
    terminate_flag: signal-sage flag used to request the termination of the thread
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