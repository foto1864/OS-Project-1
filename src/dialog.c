#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/dialog.h"

int main(int argc, char *argv[]) {

    // At first we need to check the validity of the input arguments.
    int dialog_id = check_argument_validity(argc, argv);

    // Create and attach shared memory, we get back shm_id for future cleanup.
    int shm_id = -1;
    shared_memory_t *shared_mem = create_and_attach_shared_mem(&shm_id);

    // Create and open semaphore 'mutex' which works as an inter-process lock.
    // This will protect the shared memory from simultaneous modification.
    mutex = sem_open("/mutex", O_CREAT, 0666, 1);
    if (mutex == SEM_FAILED) {
        perror("sem_open /mutex");
        exit(errno);
    }

    // Critical section, because we are going to modify shared parts of memory: wait for mutex semaphore
    sem_wait(mutex);

    // If the shared memory hasn't yet been initialized we initialize it only once.
    // The mutex semaphore ensures that there won't be 2+ simultaneous initializations
    // from different processes, which will cause corruption.

    if (!shared_mem->is_initialized) {
        init_shared_memory(shared_mem);
    }

    // We spot the dialog with <dialog_ID> and if it is not active we make it active.
    // This happens inside the same critical section, ensuring that the state is consistent
    // if many processes enter.

    dialog_t *dialog = &shared_mem->dialogs[dialog_id];
    if (!dialog->is_active) {
        dialog->is_active = 1;
        dialog->dialog_id = dialog_id;
    }

    // The calling process enters the dialog with 'join_dialog' and gets a slot inside it.
    // This data is all shared among processes, which is why this too needs to happen inside a mutex.

    int my_slot = join_dialog(dialog, getpid());
    shared_mem->total_processes++;

    // End of critical section because the state of the dialog and the entry 
    // of the participant are solidified: mutex semaphore posts it
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
    args.shm_id = shm_id;

    // We create a terminate_flag, which will be local to the calling process, and will be used 
    // to ensure synchronization between the two threads of the calling process. 
    // The reader thread will raise this flag if it sees the message "TERMINATE"
    // The writer thread will periodically check it in order to not be stuck in stdin and terminate as well.

    volatile sig_atomic_t terminate_flag = 0;
    args.terminate_flag = &terminate_flag;

    // Create threads and pass the arguments to them
    pthread_t reader_tid, writer_tid;

    if (pthread_create(&reader_tid, NULL, reader_thread, &args) != 0) {
        perror("pthread_create reader");
        exit(errno);
    }

    if (pthread_create(&writer_tid, NULL, writer_thread, &args) != 0) {
        perror("pthread_create writer");
        exit(errno);
    }

    // Join the threads to the main, so that main wait until the I/O loop is finished (either with TERMINATE or EOF)
    pthread_join(writer_tid, NULL);
    pthread_join(reader_tid, NULL);

    // Before a process leaves the dialog, we need to enter a critical section again, in order to signify to the shared
    // memory that a process is leaving. We release the slot of the dialog (the 1 of the 32 total), and we decrease the
    // total_processes variable (inside leave_dialog). We check if the dialog is the last one remaining, in order to 
    // ensure that the global cleanup only happens once all processes have terminated.

    sem_wait(mutex);
    leave_dialog(shared_mem, dialog, my_slot);
    int last = (shared_mem->total_processes == 0);
    sem_post(mutex);

    // If indeed this is the last process, we do the global cleanup and return 0 to the main.
    // What exactly happens during the global cleanup is mentioned in utils.h in the function's description.

    if (last) {
        sem_wait(mutex);
        cleanup_if_last_process(shared_mem, shm_id);
        return 0;
    }

    // If there still are processes that are alive, we do only per-process clean up.
    // Detach from the shared memory and close the mutex file descriptor only for this 
    // process as there are still other processes using it.

    shmdt(shared_mem);
    sem_close(mutex);

    return 0;
}