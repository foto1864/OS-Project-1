/* src/threads.c */
#include <stdio.h>
#include <string.h>
#include "../include/threads.h"
#include "../include/utils.h"

static void trim_newline(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    if (n == 0) return;
    if (s[n - 1] == '\n') s[n - 1] = '\0';
}

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

int message_receive(dialog_t *dialog, int my_slot) {
    int read_count = 0;

    for (int i = 0; i < MAX_MSGS_PER_DIALOG; i++) {
        message_t *m = &dialog->messages[i];
        if (!m->message_exists) continue;
        if (m->has_been_read_by[my_slot]) continue;

        printf("[dialog %d] (slot %d, pid %d): %s\n",
               m->dialog_id, m->sender_slot, (int)m->sender_pid, m->msg_text);

        m->has_been_read_by[my_slot] = 1;
        read_count++;
    }

    return read_count;
}

void* reader_thread(void *arg) {
    thread_args_t *a = (thread_args_t*)arg;

    while (1) {
        if (sem_wait(msg_available) == -1) continue;

        if (sem_wait(mutex) == -1) continue;
        int got = message_receive(a->dialog, a->my_slot);
        (void)got;
        sem_post(mutex);
    }

    return NULL;
}

void* writer_thread(void *arg) {
    thread_args_t *a = (thread_args_t*)arg;
    char buf[MAX_MSG_SIZE];

    while (1) {
        if (fgets(buf, sizeof(buf), stdin) == NULL) break;
        trim_newline(buf);

        if (buf[0] == '\0') continue;
        if (strcmp(buf, "/exit") == 0 || strcmp(buf, "/quit") == 0) break;

        if (sem_wait(mutex) == -1) continue;

        int rc = message_send(a->dialog, a->my_slot, buf);
        int participants = a->dialog->dialog_participants;

        sem_post(mutex);

        if (rc == 0) {
            int wake = participants - 1;
            if (wake < 0) wake = 0;
            for (int i = 0; i < wake; i++) sem_post(msg_available);
        }
    }

    if (sem_wait(mutex) != -1) {
        if (a->dialog->slot_used[a->my_slot]) {
            a->dialog->slot_used[a->my_slot] = 0;
            a->dialog->pids[a->my_slot] = 0;
            if (a->dialog->dialog_participants > 0) a->dialog->dialog_participants--;
            if (a->dialog->dialog_participants == 0) a->dialog->is_active = 0;
        }
        sem_post(mutex);
    }

    _exit(0);
    return NULL;
}
