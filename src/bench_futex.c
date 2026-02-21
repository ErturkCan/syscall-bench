#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syscall.h>
#include <linux/futex.h>
#include <pthread.h>
#include <string.h>
#include "../include/bench.h"

/* Futex syscall wrapper */
static long futex(uint32_t *uaddr, int futex_op, uint32_t val,
                   const struct timespec *timeout, uint32_t *uaddr2, uint32_t val3) {
    return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

typedef struct {
    uint32_t *futex_word;
    pthread_barrier_t *barrier;
    histogram_t *hist;
    int iterations;
    int same_core;
} futex_thread_args_t;

void* futex_waiter_thread(void *arg) {
    futex_thread_args_t *args = (futex_thread_args_t *)arg;

    if (args->same_core) {
        /* Pin to same core as main thread */
        if (cpu_pin_current(0) < 0) {
            perror("cpu_pin_current");
            return NULL;
        }
    } else {
        /* Pin to different core */
        int nprocs = cpu_get_nprocs();
        if (nprocs < 2) {
            fprintf(stderr, "Need at least 2 CPUs for cross-core test\n");
            return NULL;
        }
        if (cpu_pin_current(nprocs - 1) < 0) {
            perror("cpu_pin_current");
            return NULL;
        }
    }

    /* Synchronize with main thread */
    pthread_barrier_wait(args->barrier);

    timing_t timing;

    for (int i = 0; i < args->iterations && args->hist->sample_count < MAX_BENCHMARK_SAMPLES; i++) {
        TIMING_START(timing);
        futex(args->futex_word, FUTEX_WAIT_PRIVATE, 0, NULL, NULL, 0);
        TIMING_STOP(timing);

        ns_t latency = timing_elapsed_ns(timing);
        histogram_add_sample(args->hist, latency);
    }

    return (void*)0;
}

int bench_futex(int iterations, histogram_t *hist) {
    uint32_t futex_word = 0;
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, 2);

    /* Pin main thread to CPU 0 */
    if (cpu_pin_current(0) < 0) {
        perror("cpu_pin_current");
        pthread_barrier_destroy(&barrier);
        return -1;
    }

    /* Test 1: Same-core futex wake */
    printf("  Measuring same-core futex latency...\n");

    futex_thread_args_t same_core_args = {
        .futex_word = (uint32_t*)&futex_word,
        .barrier = &barrier,
        .hist = hist,
        .iterations = iterations,
        .same_core = 1
    };

    pthread_t thread1;
    if (pthread_create(&thread1, NULL, futex_waiter_thread, &same_core_args) != 0) {
        perror("pthread_create");
        pthread_barrier_destroy(&barrier);
        return -1;
    }

    /* Synchronize */
    pthread_barrier_wait(&barrier);

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        futex(&futex_word, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
        usleep(100);
    }

    /* Measure */
    timing_t timing;
    for (int i = 0; i < iterations && hist->sample_count < MAX_BENCHMARK_SAMPLES; i++) {
        TIMING_START(timing);
        futex(&futex_word, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
        TIMING_STOP(timing);

        ns_t latency = timing_elapsed_ns(timing);
        histogram_add_sample(hist, latency);

        usleep(100);
    }

    pthread_join(thread1, NULL);

    /* Test 2: Cross-core futex wake */
    printf("  Measuring cross-core futex latency...\n");

    futex_word = 0;
    pthread_barrier_destroy(&barrier);
    pthread_barrier_init(&barrier, NULL, 2);

    futex_thread_args_t cross_core_args = {
        .futex_word = (uint32_t*)&futex_word,
        .barrier = &barrier,
        .hist = hist,
        .iterations = iterations,
        .same_core = 0
    };

    if (pthread_create(&thread1, NULL, futex_waiter_thread, &cross_core_args) != 0) {
        perror("pthread_create");
        pthread_barrier_destroy(&barrier);
        return -1;
    }

    /* Synchronize */
    pthread_barrier_wait(&barrier);

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        futex(&futex_word, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
        usleep(100);
    }

    /* Measure */
    for (int i = 0; i < iterations && hist->sample_count < MAX_BENCHMARK_SAMPLES; i++) {
        TIMING_START(timing);
        futex(&futex_word, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
        TIMING_STOP(timing);

        ns_t latency = timing_elapsed_ns(timing);
        histogram_add_sample(hist, latency);

        usleep(100);
    }

    pthread_join(thread1, NULL);
    pthread_barrier_destroy(&barrier);

    return 0;
}
