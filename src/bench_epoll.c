#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <string.h>
#include <errno.h>
#include "../include/bench.h"

/* Benchmark epoll_wait latency with configurable number of file descriptors */
int bench_epoll(int fd_count, int iterations, histogram_t *hist) {
    if (fd_count < 1 || fd_count > 10000) {
        fprintf(stderr, "Invalid fd_count: %d\n", fd_count);
        return -1;
    }

    /* Create pipe pairs */
    int *pipes = malloc(fd_count * 2 * sizeof(int));
    if (!pipes) {
        perror("malloc");
        return -1;
    }

    for (int i = 0; i < fd_count; i++) {
        if (pipe(&pipes[i * 2]) < 0) {
            perror("pipe");
            free(pipes);
            return -1;
        }
    }

    /* Create epoll instance */
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        perror("epoll_create1");
        free(pipes);
        return -1;
    }

    /* Register all read ends with epoll */
    for (int i = 0; i < fd_count; i++) {
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.u64 = i;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, pipes[i * 2], &ev) < 0) {
            perror("epoll_ctl");
            close(epfd);
            free(pipes);
            return -1;
        }
    }

    struct epoll_event events[fd_count];
    timing_t timing;

    /* Warmup phase */
    for (int i = 0; i < WARMUP_ITERATIONS && hist->sample_count < MAX_BENCHMARK_SAMPLES; i++) {
        /* Trigger one event by writing to a pipe */
        char byte = 'X';
        int pipe_idx = i % fd_count;
        if (write(pipes[pipe_idx * 2 + 1], &byte, 1) < 0) {
            perror("write warmup");
        }

        TIMING_START(timing);
        int nready = epoll_wait(epfd, events, fd_count, 1);
        TIMING_STOP(timing);

        if (nready < 0) {
            perror("epoll_wait warmup");
        }

        /* Drain the pipe */
        char drain[256];
        while (read(pipes[pipe_idx * 2], drain, sizeof(drain)) > 0);
    }

    /* Measurement phase */
    for (int i = 0; i < iterations && hist->sample_count < MAX_BENCHMARK_SAMPLES; i++) {
        /* Trigger one event by writing to a pipe */
        char byte = 'X';
        int pipe_idx = i % fd_count;
        if (write(pipes[pipe_idx * 2 + 1], &byte, 1) < 0) {
            perror("write");
            close(epfd);
            free(pipes);
            return -1;
        }

        TIMING_START(timing);
        int nready = epoll_wait(epfd, events, fd_count, 1);
        TIMING_STOP(timing);

        if (nready < 0) {
            perror("epoll_wait");
            close(epfd);
            free(pipes);
            return -1;
        }

        ns_t latency = timing_elapsed_ns(timing);
        histogram_add_sample(hist, latency);

        /* Drain the pipe */
        char drain[256];
        while (read(pipes[pipe_idx * 2], drain, sizeof(drain)) > 0);
    }

    /* Cleanup */
    close(epfd);
    for (int i = 0; i < fd_count; i++) {
        close(pipes[i * 2]);
        close(pipes[i * 2 + 1]);
    }
    free(pipes);

    return 0;
}
