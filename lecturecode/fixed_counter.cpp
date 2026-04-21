#include <pthread.h>
#include <stdio.h>

int counter = 0;
pthread_mutex_t lock;

// discussion 4-17-26

void* worker(void* arg) {
    for (int i = 0; i < 100000; i++) {
        pthread_mutex_lock(&lock);
        counter++;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;
    pthread_mutex_init(&lock, NULL);

    pthread_create()
}