#ifndef UTILS_H
#define UTILS_H

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_MSG_SIZE 256
#define MAX_MSGS_PER_DIALOG 512
#define MAX_DIALOG_PARTICIPANTS 16
#define MAX_DIALOGS 32

#define LIMIT_REACHED 10

typedef struct {
    int message_exists;
    int dialog_id;
    int sender_slot;
    pid_t sender_pid;
    char msg_text[MAX_MSG_SIZE];
    int has_been_read_by[MAX_DIALOG_PARTICIPANTS];
} message_t;

typedef struct {
    int is_active;
    int dialog_id;
    int dialog_participants;
    int slot_used[MAX_DIALOG_PARTICIPANTS];
    pid_t pids[MAX_DIALOG_PARTICIPANTS];
    sem_t slot_sem[MAX_DIALOG_PARTICIPANTS];
    int sem_initialized;
    message_t messages[MAX_MSGS_PER_DIALOG];
} dialog_t;

typedef struct {
    int is_initialized;
    int total_processes;
    dialog_t dialogs[MAX_DIALOGS];
} shared_memory_t;

extern sem_t *mutex;

/*
*/
int check_argument_validity(int argc, char* argv[]);

shared_memory_t* create_and_attach_shared_mem(int *out_shm_id);
void init_shared_memory(shared_memory_t *shared_mem);

int join_dialog(dialog_t *dialog, pid_t pid);
void leave_dialog(shared_memory_t *shared_mem, dialog_t *dialog, int my_slot);

void cleanup_if_last_process(shared_memory_t *shared_mem, int shm_id);

#endif
