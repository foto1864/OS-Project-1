/* src/utils.c */
#include "../include/utils.h"

sem_t *mutex = NULL;
sem_t *msg_available = NULL;

int join_dialog(dialog_t *dialog, pid_t pid) {
    for (int i = 0; i < MAX_DIALOG_PARTICIPANTS; i++) {
        if (dialog->slot_used[i] == 0) {
            dialog->slot_used[i] = 1;
            dialog->pids[i] = pid;
            dialog->dialog_participants++;
            return i;
        }
    }
    fprintf(stderr, "Dialog with ID:%d is full, cannot join.\n", dialog->dialog_id);
    exit(4);
}

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

int check_argument_validity(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Error: Expected exactly 2 arguments.\nUsage: ./dialog <dialog_ID>\n");
        exit(1);
    }

    if (argv[1][0] == '\0') {
        fprintf(stderr, "Error: Empty <dialog_ID>.\n");
        exit(2);
    }

    size_t n = strlen(argv[1]);
    for (size_t i = 0; i < n; i++) {
        if (argv[1][i] < '0' || argv[1][i] > '9') {
            fprintf(stderr, "Error: Invalid input in argument 1.\nPlease input an integer in the range [0,31] for <dialog_ID>\n");
            exit(2);
        }
    }

    long dialog_id = strtol(argv[1], NULL, 10);
    if (dialog_id < 0 || dialog_id >= MAX_DIALOGS) {
        fprintf(stderr, "Dialog ID should be in the range [0,%d]\n", MAX_DIALOGS - 1);
        exit(3);
    }

    return (int)dialog_id;
}

shared_memory_t* create_and_attach_shared_mem(void) {
    key_t mem_key = 612004;

    int shm_id = shmget(mem_key, sizeof(shared_memory_t), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget");
        exit(errno);
    }

    shared_memory_t *shared_mem = shmat(shm_id, NULL, 0);
    if (shared_mem == (void*) -1) {
        perror("shmat");
        exit(errno);
    }
    return shared_mem;
}
