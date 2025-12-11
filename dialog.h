
#include <pthread.h>
#include <semaphore.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
#include <errno.h>

#define MAX_MSG_SIZE 256
#define MAX_MSGS_PER_DIALOG 512
#define MAX_DIALOG_PARTICIPANTS 16
#define MAX_DIALOGS 32

#define LIMIT_REACHED 10 // error code for dialog reaching max message capacity

typedef struct {
    int message_exists; // actually a bool
    int dialog_id; 
    int sender_slot;
    pid_t sender_pid; 
    char msg_text[MAX_MSG_SIZE];
    int has_been_read_by[MAX_DIALOG_PARTICIPANTS]; // actually a bool
} message_t;

typedef struct {
    int is_active; // actually a bool
    int dialog_id;
    int dialog_participants;
    int slot_used[MAX_DIALOG_PARTICIPANTS];
    pid_t pids[MAX_DIALOG_PARTICIPANTS];
    message_t messages[MAX_MSGS_PER_DIALOG];
} dialog_t;

typedef struct {
    int is_initialized;
    dialog_t dialogs[MAX_DIALOGS];
} shared_memory_t;

sem_t *mutex;
sem_t *msg_available;

void init_shared_memory(shared_memory_t *shared_mem);
int join_dialog(dialog_t *dialog, pid_t pid);

typedef struct {
    shared_memory_t *shared_mem;
    dialog_t *dialog;
    int dialog_id;
    int my_slot;
} thread_args_t;

void* reader_thread(void* arg);
void* writer_thread(void* arg);

int message_send(dialog_t *dialog, int my_slot, const char *text);
int message_receive(dialog_t *dialog, int my_slot);