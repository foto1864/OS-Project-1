#include <stdio.h>
#include <string.h>
#include "../include/threads.h"
#include "../include/utils.h"

#include <poll.h>

static void trim_newline(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    if (n == 0) return;
    if (s[n - 1] == '\n') s[n - 1] = '\0';
}

static void cleanup_read_messages(dialog_t *dialog) {
    for (int i = 0; i < MAX_MSGS_PER_DIALOG; i++) {
        message_t *m = &dialog->messages[i];
        if (!m->message_exists) continue;

        int all_read = 1;
        for (int s = 0; s < MAX_DIALOG_PARTICIPANTS; s++) {
            if (!dialog->slot_used[s]) continue;                 // ignore inactive slots
            if (!m->has_been_read_by[s]) {                       // at least one active participant hasn't read it yet
                all_read = 0;
                break;
            }
        }

        if (all_read) {
            m->message_exists = 0;                               // free this message slot for reuse
            m->sender_slot = -1;
            m->sender_pid = 0;
            m->msg_text[0] = '\0';
            for (int s = 0; s < MAX_DIALOG_PARTICIPANTS; s++) {
                m->has_been_read_by[s] = 0;                      // reset read-tracking state
            }
        }
    }
}

int message_send(dialog_t *dialog, int my_slot, const char *text) {
    int index = -1;
    for (int i = 0; i < MAX_MSGS_PER_DIALOG; i++) {
        if (dialog->messages[i].message_exists == 0) {           // first free message slot
            index = i;
            break;
        }
    }
    if (index == -1) {
        fprintf(stderr, "Dialog %d reached message limit.\n", dialog->dialog_id);
        return LIMIT_REACHED;                                    // mailbox full
    }

    message_t *m = &dialog->messages[index];
    m->message_exists = 1;
    m->dialog_id = dialog->dialog_id;
    m->sender_slot = my_slot;
    m->sender_pid = dialog->pids[my_slot];

    strncpy(m->msg_text, text, MAX_MSG_SIZE - 1);                // bounded copy to avoid overflow
    m->msg_text[MAX_MSG_SIZE - 1] = '\0';                        // ensure NUL-termination

    for (int s = 0; s < MAX_DIALOG_PARTICIPANTS; s++) {
        m->has_been_read_by[s] = 0;                              // nobody has read it yet
    }

    return 0;
}

int message_receive(dialog_t *dialog, int my_slot) {
    int read_count = 0;
    int saw_terminate = 0;

    for (int i = 0; i < MAX_MSGS_PER_DIALOG; i++) {
        message_t *m = &dialog->messages[i];
        if (!m->message_exists) continue;
        if (m->has_been_read_by[my_slot]) continue;              // only consume unread messages for this slot

        printf("[dialog %d] (slot %d, pid %d): %s\n",
               m->dialog_id, m->sender_slot, (int)m->sender_pid, m->msg_text);
        fflush(stdout);                                          // avoid buffered output in multi-process runs

        m->has_been_read_by[my_slot] = 1;                        // mark as read for this participant
        read_count++;

        if (strcmp(m->msg_text, "TERMINATE") == 0) saw_terminate = 1;
    }

    cleanup_read_messages(dialog);                               // reclaim messages fully read by all active participants

    return saw_terminate ? -1 : read_count;                      // -1 is used as a termination signal to the reader thread
}

void* reader_thread(void *arg) {
    thread_args_t *a = (thread_args_t*)arg;

    sem_wait(mutex);
    message_receive(a->dialog, a->my_slot);                      // print any messages that existed before this reader started
    sem_post(mutex);

    while (1) {
        sem_wait(&a->dialog->slot_sem[a->my_slot]);              // sleep until someone posts that this slot has new activity

        sem_wait(mutex);                                         // protect shared memory while scanning/marking messages
        int got = message_receive(a->dialog, a->my_slot);
        sem_post(mutex);

        if (got == -1) {                                         // saw TERMINATE
            *a->terminate_flag = 1;                              // tell writer thread to stop as well
            break;
        }
    }

    return NULL;
}

void* writer_thread(void *arg) {
    thread_args_t *a = (thread_args_t*)arg;
    char buf[MAX_MSG_SIZE];

    while (1) {
        if (*a->terminate_flag) break;                           // termination requested by reader or self

        struct pollfd pfd;
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;

        int pr = poll(&pfd, 1, 200);                             // timeout keeps the loop responsive to terminate_flag
        if (pr <= 0) continue;

        if (fgets(buf, sizeof(buf), stdin) == NULL) break;       // EOF / input closed
        trim_newline(buf);

        if (buf[0] == '\0') continue;                            // ignore empty messages

        sem_wait(mutex);

        int rc = message_send(a->dialog, a->my_slot, buf);

        if (rc == 0) {
            for (int s = 0; s < MAX_DIALOG_PARTICIPANTS; s++) {
                if (a->dialog->slot_used[s]) {
                    sem_post(&a->dialog->slot_sem[s]);           // wake all active participants to check for new messages
                }
            }
        }

        sem_post(mutex);

        if (strcmp(buf, "TERMINATE") == 0) {
            *a->terminate_flag = 1;                              // stop this writer; reader will also exit when it sees it
            break;
        }
    }

    return NULL;
}
