/* src/threads.c */
#include <stdio.h>
#include <string.h>
#include "../include/threads.h"
#include "../include/utils.h"

int message_send(dialog_t *dialog, int my_slot, const char *text) {
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

    message_t *message = &dialog->messages[index];
    message->message_exists = 1;
    message->dialog_id = dialog->dialog_id;
    message->sender_slot = my_slot;
    message->sender_pid = dialog->pids[my_slot];

    strncpy(message->msg_text, text, MAX_MSG_SIZE - 1);
    message->msg_text[MAX_MSG_SIZE - 1] = '\0';

    for (int i = 0; i < MAX_DIALOG_PARTICIPANTS; i++) {
        message->has_been_read_by[i] = 0;
    }

    return 0;
}

void* reader_thread(void *arg) {
    (void)arg;
    return NULL;
}

void* writer_thread(void *arg) {
    (void)arg;
    return NULL;
}
