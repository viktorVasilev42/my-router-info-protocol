#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

void* print_message(void* thread_id) {
    uint64_t tid = (uint64_t) thread_id;
    if (tid == 1) {
        sleep(1);
    }
    printf("Thread receieved number: %lu\n", tid);
    return NULL;
}

int main() {
    const uint32_t NUM_THREADS = 5;
    pthread_t threads[NUM_THREADS];

    for (uint64_t i = 0; i < NUM_THREADS; i++) {
        int rc = pthread_create(&threads[i], NULL, print_message, (void*) i);

        if (rc) {
            printf("Error");
            exit(EXIT_FAILURE);
        }
    }

    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("All threads finished\n");
    pthread_exit(NULL);
}
