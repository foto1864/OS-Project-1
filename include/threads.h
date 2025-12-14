#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>
#include <signal.h>

#include "utils.h"

typedef struct {
    shared_memory_t *shared_mem;
    dialog_t *dialog;
    int dialog_id;
    int my_slot;
    int shm_id;
    volatile sig_atomic_t *terminate_flag;
} thread_args_t;

void* reader_thread(void* arg);
void* writer_thread(void* arg);

int message_send(dialog_t *dialog, int my_slot, const char *text);
int message_receive(dialog_t *dialog, int my_slot);

#endif
