#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dialog.h"

int main(int argc, char *argv[]) {

    // Check the validity of all arguments 
    if (argc != 2) {
        fprintf(stderr, "Error: Expected exactly 2 arguments.\nUsage: ./dialog <dialog_ID>\n");
        exit(1);
    }

    for (int i=0; i<strlen(argv[1]); i++) {
        if (argv[1][i] < '0' || argv[1][i] > '9') {
            fprintf(stderr, "Error: Invalid input in argument 1.\nPlease input an integer in the range [0,31] for <dialog_ID>\n");
            exit(2);
        }
    }

    int dialog_id = atoi(argv[1]);
    if (dialog_id >= MAX_DIALOGS) {
        fprintf(stderr, "Dialog ID should be in the range [0,31]\n");
        exit(3);
    }
    printf("Dialog id is: %d\n", dialog_id);

    // Create shared memory segment
    key_t mem_key = 612004; // My birthday as a key to avoid using ftok

    int shm_id = shmget(mem_key, sizeof(shared_memory_t), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget");
        exit(errno);
    }

    // Attach shared memory
    shared_memory_t *shared_mem = shmat(shm_id, NULL, 0);
    if (shared_mem == (void*) -1) {
        perror("shmat");
        exit(errno);
    } 

    // Create semaphores 
    mutex = sem_open("/mutex", O_CREAT, 0666, 1);
    if (mutex == SEM_FAILED) {
        perror("sem_open on mutex");
        exit(errno);
    }
    msg_available = sem_open("/msg_available", O_CREAT, 0666, 0);
    if (msg_available == SEM_FAILED) {
        perror("sem_open on msg_available");
        exit(errno);
    }

    // Initialize shared memory : Only needs to happen once, hence we use the mutex semaphore
    sem_wait(mutex);
    if (!shared_mem->is_initialized) {
        init_shared_memory(shared_mem);
        shared_mem->is_initialized = 1;
    }
    sem_post(mutex);

    // Make the participant join the dialog with <dialog_ID>
    sem_wait(mutex);

    dialog_t *dialog = &shared_mem->dialogs[dialog_id];
    if (!dialog->is_active) {
        dialog->is_active = 1;
        dialog->dialog_id = dialog_id;
    }
    int my_slot = join_dialog(dialog, getpid());

    sem_post(mutex);

    // At this point the process with id <pid> has joined the dialog with id <dialog_ID>
    // We now need to create 2 threads, one for reading and one for writing. These threads
    // need to take in as arguments: the shared memory region, the designated dialog, its 
    // dialog_id, and at last the slot in which the process was put in during the last segment.

    thread_args_t args;
    args.shared_mem = shared_mem;
    args.dialog = dialog;
    args.dialog_id = dialog_id;
    args.my_slot = my_slot;

    // Create the threads
    pthread_t reader_thread_id, writer_thread_id;

    int reader = pthread_create(&reader_thread_id, NULL, reader_thread, &args); 
    if (reader != 0) {
        perror("pthread_create on read");
        exit(errno);
    }

    int writer = pthread_create(&writer_thread_id, NULL, writer_thread, &args);
    if (writer != 0) {
        perror("pthread_create on write");
        exit(errno);
    }

    // Join the threads to the main
    int joined_r = pthread_join(reader_thread_id, NULL);
    if (joined_r != 0) {
        perror("pthread_join on read");
        exit(errno);
    }
    int joined_w = pthread_join(writer_thread_id, NULL);
    if (joined_w != 0) {
        perror("pthread_join on write");
        exit(errno);
    } 



    // Close and unlink semaphores
    // sem_close(mutex);
    // sem_close(msg_available);
    // sem_unlink("/mutex");
    // sem_unlink("/msg_available");

    // Detach shared memory
    // shmdt(shared_mem);
    // shmctl(shm_id, IPC_RMID, NULL);

    return 0;
}


/*  Each dialog can have up to MAX_DIALOG_PARTICIPANTS participants. Each participant that joins a dialog
    is a process with a unique pid. This participant fills the first empty slot of the dialogs (slots_used)
    unless all the slots have been filled. We use "join_dialog" in order to find which slot is the first empty
    one in the dialog and insert the process there.
*/
int join_dialog(dialog_t *dialog, pid_t pid) {
    for (int i = 0; i < MAX_DIALOG_PARTICIPANTS; i++) {
        if (dialog->slot_used[i] == 0) {
            dialog->slot_used[i] = 1; // now the slot is full, meaning no other process can join the dialog in this slot
            dialog->pids[i] = pid;
            dialog->dialog_participants++;
            return i;
        }
    }
    fprintf(stderr, "Dialog with ID:%d is full, cannot join.\n", dialog->dialog_id);
    exit(4);
}

/*  The function "init_shared_memory" is called once after the first process invokes main (./dialog <dialog_ID>). 
    This function's purpose is to initialize all the data in the shared memory, clearing all dialogs and all messages
    that are contained in it. The kernel probably already does this by default, but it is a good practice.
*/
void init_shared_memory(shared_memory_t *shared_mem) {
    
    shared_mem->is_initialized = 0;

    for (int i = 0; i < MAX_DIALOGS; i++) {
        dialog_t *dialog = &shared_mem->dialogs[i];
        dialog->is_active = 0;
        dialog->dialog_id = i;
        dialog->dialog_participants = 0;

        for (int j = 0; j < MAX_DIALOG_PARTICIPANTS; j++) {
            dialog->slot_used[j] = 0;
            dialog->pids[j] = 0;
        }

        for (int k = 0; k < MAX_MSGS_PER_DIALOG; k++) {
            message_t *msg = &dialog->messages[k];
            msg->message_exists = 0;
            msg->dialog_id = i;
            msg->sender_slot = -1;
            msg->sender_pid = 0;
            msg->msg_text[0] = '\0';

            for (int p = 0; p < MAX_DIALOG_PARTICIPANTS; p++) {
                msg->has_been_read_by[p] = 0;
            }
        }
    }
}

int message_send(dialog_t *dialog, int my_slot, const char *text) {
    
    // At first we find the index where the new message needs to be put
    int index = -1;
    for (int i = 0; i < MAX_MSGS_PER_DIALOG; i++) {
        if (dialog->messages[i].message_exists == 0) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        fprintf(stderr, "Dialog with ID:%d has reached its message limit.\n", dialog->dialog_id);
        return LIMIT_REACHED;
    }

    // Put the message in the dialog messages array
    message_t *message = &dialog->messages[index];
    message->message_exists = 1;
    message->dialog_id = dialog->dialog_id;
    message->sender_slot = my_slot;
    message->sender_pid = dialog->pids[my_slot];
    strncpy(message->msg_text, text, MAX_MSG_SIZE - 1);
    message->msg_text[MAX_MSG_SIZE] = '\0';
    
    // We signify that the message hasn't been read by anyone yet
    for (int i = 0; i < MAX_DIALOG_PARTICIPANTS; i++) {
        message->has_been_read_by[i] = 0;
    }

    // Return 0 to signify success in the execution of "message_send"
    return 0; 
}

void* reader_thread(void *arg) {
    return NULL;
}

void* writer_thread(void *arg) {
    return NULL;
}