#include <stdio.h>
#include <stdlib.h>

#include "../include/threads.h"
#include "../include/utils.h"

int main(int argc, char *argv[]) {

    int dialog_id = check_argument_validity(argc, argv);
    printf("Dialog id is: %d\n", dialog_id);

    shared_memory_t *shared_mem = create_and_attach_shared_mem();
    printf("Shared mem created successfully\n");

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