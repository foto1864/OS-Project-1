#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define MAX_MSG 256

typedef struct {
    char message[MAX_MSG];
    sem_t mutex;
    sem_t msg_avail;
    int terminate;
} shared_t;

shared_t shared;

void *input_thread(void *arg) {
    char line[MAX_MSG];
    while (1) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }

        sem_wait(&shared.mutex);
        strncpy(shared.message, line, MAX_MSG - 1);
        shared.message[MAX_MSG - 1] = '\0';

        if (strncmp(line, "TERMINATE", 9) == 0) {
            shared.terminate = 1;
        }

        sem_post(&shared.msg_avail);
        sem_post(&shared.mutex);

        if (shared.terminate) {
            break;
        }
    }
    return NULL;
}

void *listener_thread(void *arg) {
    while (1) {
        sem_wait(&shared.msg_avail);

        sem_wait(&shared.mutex);
        printf("[Listener] Received: %s", shared.message);
        int stop = shared.terminate;
        sem_post(&shared.mutex);

        if (stop) {
            printf("[Listener] TERMINATE received, exiting listener.\n");
            break;
        }
    }
    return NULL;
}

int main(void) {
    pthread_t t_input, t_listener;

    shared.terminate = 0;
    if (sem_init(&shared.mutex, 0, 1) != 0) {
        perror("sem_init mutex");
        exit(1);
    }
    if (sem_init(&shared.msg_avail, 0, 0) != 0) {
        perror("sem_init msg_avail");
        exit(1);
    }

    if (pthread_create(&t_input, NULL, input_thread, NULL) != 0) {
        perror("pthread_create input");
        exit(1);
    }
    if (pthread_create(&t_listener, NULL, listener_thread, NULL) != 0) {
        perror("pthread_create listener");
        exit(1);
    }

    pthread_join(t_input, NULL);
    pthread_join(t_listener, NULL);

    return 0;
}
