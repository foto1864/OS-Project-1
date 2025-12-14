#include "../include/utils.h"

sem_t *mutex = NULL;

int check_argument_validity(int argc, char* argv[]) {
    if (argc != 2) {                                             // require exactly one argument: the dialog ID
        fprintf(stderr, "Usage: ./dialog <dialog_ID>\n");
        exit(1);
    }
    if (argv[1][0] == '\0') {                                    // reject empty string early
        fprintf(stderr, "Error: Empty <dialog_ID>.\n");
        exit(2);
    }
    size_t n = strlen(argv[1]);
    for (size_t i = 0; i < n; i++) {
        if (argv[1][i] < '0' || argv[1][i] > '9') {              // enforce numeric-only input (no signs, no spaces)
            fprintf(stderr, "Error: <dialog_ID> must be an integer in [0,%d]\n", MAX_DIALOGS - 1);
            exit(2);
        }
    }
    int dialog_id = atoi(argv[1]);                               // safe after digit-only validation
    if (dialog_id < 0 || dialog_id >= MAX_DIALOGS) {             // keep ID within array bounds
        fprintf(stderr, "Error: <dialog_ID> must be in [0,%d]\n", MAX_DIALOGS - 1);
        exit(3);
    }
    return dialog_id;
}

shared_memory_t* create_and_attach_shared_mem(int *out_shm_id) {
    key_t mem_key = 612004;                                      // fixed key so all processes attach to the same segment

    int shm_id = shmget(mem_key, sizeof(shared_memory_t), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget");
        exit(errno);
    }

    void *p = shmat(shm_id, NULL, 0);
    if (p == (void*)-1) {
        perror("shmat");
        exit(errno);
    }

    if (out_shm_id) *out_shm_id = shm_id;                        // return shm id to caller for later cleanup
    return (shared_memory_t*)p;
}

void init_shared_memory(shared_memory_t *shared_mem) {
    shared_mem->total_processes = 0;                             // reset global process counter

    for (int i = 0; i < MAX_DIALOGS; i++) {
        dialog_t *dialog = &shared_mem->dialogs[i];
        dialog->is_active = 0;
        dialog->dialog_id = i;                                   // stable ID equals index
        dialog->dialog_participants = 0;

        for (int j = 0; j < MAX_DIALOG_PARTICIPANTS; j++) {
            dialog->slot_used[j] = 0;                            // all participant slots start free
            dialog->pids[j] = 0;
        }

        dialog->sem_initialized = 1;                             // mark semaphores as usable for cleanup logic
        for (int s = 0; s < MAX_DIALOG_PARTICIPANTS; s++) {
            if (sem_init(&dialog->slot_sem[s], 1, 0) != 0) {      // pshared=1 => usable across processes, initial value 0
                perror("sem_init");
                exit(errno);
            }
        }

        for (int k = 0; k < MAX_MSGS_PER_DIALOG; k++) {
            message_t *msg = &dialog->messages[k];
            msg->message_exists = 0;                             // mailbox starts empty
            msg->dialog_id = i;
            msg->sender_slot = -1;
            msg->sender_pid = 0;
            msg->msg_text[0] = '\0';
            for (int p = 0; p < MAX_DIALOG_PARTICIPANTS; p++) {
                msg->has_been_read_by[p] = 0;                    // no reads registered
            }
        }
    }

    shared_mem->is_initialized = 1;                              // publish "ready" state last
}

int join_dialog(dialog_t *dialog, pid_t pid) {
    for (int i = 0; i < MAX_DIALOG_PARTICIPANTS; i++) {
        if (dialog->slot_used[i] == 0) {                         // pick first free slot
            dialog->slot_used[i] = 1;
            dialog->pids[i] = pid;
            dialog->dialog_participants++;
            return i;                                            // caller stores this slot as its identity in the dialog
        }
    }
    fprintf(stderr, "Dialog %d is full.\n", dialog->dialog_id);
    exit(4);
}

void leave_dialog(shared_memory_t *shared_mem, dialog_t *dialog, int my_slot) {
    if (dialog->slot_used[my_slot]) {                            // only change state if the slot was actually in use
        dialog->slot_used[my_slot] = 0;
        dialog->pids[my_slot] = 0;
        if (dialog->dialog_participants > 0) dialog->dialog_participants--;
        if (dialog->dialog_participants == 0) dialog->is_active = 0; // mark dialog reusable when empty
    }
    if (shared_mem->total_processes > 0) shared_mem->total_processes--;
}

void cleanup_if_last_process(shared_memory_t *shared_mem, int shm_id) {
    if (shared_mem->total_processes != 0) return;                // only the last exiting process performs global cleanup

    for (int i = 0; i < MAX_DIALOGS; i++) {
        dialog_t *dialog = &shared_mem->dialogs[i];
        if (dialog->sem_initialized) {
            for (int s = 0; s < MAX_DIALOG_PARTICIPANTS; s++) {
                sem_destroy(&dialog->slot_sem[s]);               // destroy unnamed semaphores stored inside shared memory
            }
            dialog->sem_initialized = 0;
        }
    }

    shared_mem->is_initialized = 0;

    sem_close(mutex);                                            // close this process's handle to the named mutex
    sem_unlink("/mutex");                                        // remove the named semaphore from the system

    shmdt(shared_mem);                                           // detach mapping from this process
    shmctl(shm_id, IPC_RMID, NULL);                               // mark shared memory segment for removal
}
