#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <fcntl.h>
#include "../include/bench.h"

/* Measure pipe read/write latency */
static int bench_pipe_latency(histogram_t *hist, int iterations, int payload_size) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        return -1;
    }

    char *buf = malloc(payload_size);
    char *read_buf = malloc(payload_size);
    if (!buf || !read_buf) {
        perror("malloc");
        free(buf);
        free(read_buf);
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    memset(buf, 'X', payload_size);

    timing_t timing;

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        TIMING_START(timing);
        if (write(pipefd[1], buf, payload_size) < 0) {
            perror("write warmup");
        }
        TIMING_STOP(timing);
        if (read(pipefd[0], read_buf, payload_size) < 0) {
            perror("read warmup");
        }
    }

    /* Measure writes */
    for (int i = 0; i < iterations && hist->sample_count < MAX_BENCHMARK_SAMPLES; i++) {
        TIMING_START(timing);
        ssize_t ret = write(pipefd[1], buf, payload_size);
        TIMING_STOP(timing);

        if (ret < 0) {
            perror("write");
            free(buf);
            free(read_buf);
            close(pipefd[0]);
            close(pipefd[1]);
            return -1;
        }

        ns_t latency = timing_elapsed_ns(timing);
        histogram_add_sample(hist, latency);

        if (read(pipefd[0], read_buf, payload_size) < 0) {
            perror("read");
        }
    }

    free(buf);
    free(read_buf);
    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
}

/* Measure socketpair read/write latency */
static int bench_socketpair_latency(histogram_t *hist, int iterations, int payload_size) {
    int sockpair[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) < 0) {
        perror("socketpair");
        return -1;
    }

    char *buf = malloc(payload_size);
    char *read_buf = malloc(payload_size);
    if (!buf || !read_buf) {
        perror("malloc");
        free(buf);
        free(read_buf);
        close(sockpair[0]);
        close(sockpair[1]);
        return -1;
    }

    memset(buf, 'X', payload_size);

    timing_t timing;

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        TIMING_START(timing);
        if (send(sockpair[0], buf, payload_size, MSG_DONTWAIT) < 0) {
            perror("send warmup");
        }
        TIMING_STOP(timing);
        recv(sockpair[1], read_buf, payload_size, 0);
    }

    /* Measure sends */
    for (int i = 0; i < iterations && hist->sample_count < MAX_BENCHMARK_SAMPLES; i++) {
        TIMING_START(timing);
        ssize_t ret = send(sockpair[0], buf, payload_size, MSG_DONTWAIT);
        TIMING_STOP(timing);

        if (ret < 0) {
            perror("send");
            free(buf);
            free(read_buf);
            close(sockpair[0]);
            close(sockpair[1]);
            return -1;
        }

        ns_t latency = timing_elapsed_ns(timing);
        histogram_add_sample(hist, latency);

        recv(sockpair[1], read_buf, payload_size, 0);
    }

    free(buf);
    free(read_buf);
    close(sockpair[0]);
    close(sockpair[1]);
    return 0;
}

/* Measure file read/write latency */
static int bench_file_latency(histogram_t *hist, int iterations, int payload_size) {
    char template[] = "/tmp/bench-io-XXXXXX";
    int fd = mkstemp(template);
    if (fd < 0) {
        perror("mkstemp");
        return -1;
    }

    char *buf = malloc(payload_size);
    char *read_buf = malloc(payload_size);
    if (!buf || !read_buf) {
        perror("malloc");
        free(buf);
        free(read_buf);
        close(fd);
        unlink(template);
        return -1;
    }

    memset(buf, 'X', payload_size);

    timing_t timing;

    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        TIMING_START(timing);
        if (write(fd, buf, payload_size) < 0) {
            perror("write warmup");
        }
        TIMING_STOP(timing);
        lseek(fd, 0, SEEK_SET);
        if (read(fd, read_buf, payload_size) < 0) {
            perror("read warmup");
        }
        lseek(fd, 0, SEEK_SET);
    }

    /* Measure writes */
    for (int i = 0; i < iterations && hist->sample_count < MAX_BENCHMARK_SAMPLES; i++) {
        TIMING_START(timing);
        ssize_t ret = write(fd, buf, payload_size);
        TIMING_STOP(timing);

        if (ret < 0) {
            perror("write");
            free(buf);
            free(read_buf);
            close(fd);
            unlink(template);
            return -1;
        }

        ns_t latency = timing_elapsed_ns(timing);
        histogram_add_sample(hist, latency);

        lseek(fd, 0, SEEK_SET);
        if (read(fd, read_buf, payload_size) < 0) {
            perror("read");
        }
        lseek(fd, 0, SEEK_SET);
    }

    free(buf);
    free(read_buf);
    close(fd);
    unlink(template);
    return 0;
}

int bench_io(int iterations, histogram_t *hist) {
    printf("  Measuring pipe write latency (64 bytes)...\n");
    if (bench_pipe_latency(hist, iterations, 64) < 0) {
        return -1;
    }

    printf("  Measuring pipe write latency (4096 bytes)...\n");
    if (bench_pipe_latency(hist, iterations / 2, 4096) < 0) {
        return -1;
    }

    printf("  Measuring socketpair send latency (64 bytes)...\n");
    if (bench_socketpair_latency(hist, iterations, 64) < 0) {
        return -1;
    }

    printf("  Measuring socketpair send latency (4096 bytes)...\n");
    if (bench_socketpair_latency(hist, iterations / 2, 4096) < 0) {
        return -1;
    }

    printf("  Measuring file write latency (64 bytes)...\n");
    if (bench_file_latency(hist, iterations, 64) < 0) {
        return -1;
    }

    printf("  Measuring file write latency (4096 bytes)...\n");
    if (bench_file_latency(hist, iterations / 2, 4096) < 0) {
        return -1;
    }

    return 0;
}
