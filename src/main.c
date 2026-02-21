#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <time.h>
#include "../include/bench.h"

/* Benchmark function signatures */
int bench_epoll(int fd_count, int iterations, histogram_t *hist);
int bench_futex(int iterations, histogram_t *hist);
int bench_io(int iterations, histogram_t *hist);

typedef struct {
    char benchmarks[256];
    int iterations;
    int fd_count;
    char output_file[256];
    int verbose;
} options_t;

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [OPTIONS]\n", prog);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -a <name>       Add benchmark: epoll, futex, or io (can be used multiple times)\n");
    fprintf(stderr, "  -c <count>      FD count for epoll benchmark (default: 10, 100, 1000)\n");
    fprintf(stderr, "  -i <iterations> Number of iterations per benchmark (default: 10000)\n");
    fprintf(stderr, "  -o <file>       JSON output file (default: results.json)\n");
    fprintf(stderr, "  -v              Verbose output\n");
    fprintf(stderr, "  -h              Show this help message\n");
}

static int write_json_output(const char *filename, bench_result_t *results, int count) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("fopen");
        return -1;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"timestamp\": %ld,\n", time(NULL));
    fprintf(f, "  \"benchmarks\": [\n");

    for (int i = 0; i < count; i++) {
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", results[i].name);
        fprintf(f, "      \"iterations\": %lu,\n", results[i].total_iterations);
        fprintf(f, "      \"duration_sec\": %.6f,\n", results[i].duration_sec);
        fprintf(f, "      \"min_ns\": %lu,\n", results[i].hist->min);
        fprintf(f, "      \"max_ns\": %lu,\n", results[i].hist->max);
        fprintf(f, "      \"p50_ns\": %.0f,\n", histogram_percentile(results[i].hist, 50.0));
        fprintf(f, "      \"p90_ns\": %.0f,\n", histogram_percentile(results[i].hist, 90.0));
        fprintf(f, "      \"p99_ns\": %.0f,\n", histogram_percentile(results[i].hist, 99.0));
        fprintf(f, "      \"p999_ns\": %.0f\n", histogram_percentile(results[i].hist, 99.9));
        fprintf(f, "    }%s\n", i < count - 1 ? "," : "");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);
    return 0;
}

int main(int argc, char *argv[]) {
    options_t opts = {
        .benchmarks = {0},
        .iterations = 10000,
        .fd_count = 0,
        .verbose = 0
    };
    strcpy(opts.output_file, "results.json");

    int opt;
    while ((opt = getopt(argc, argv, "a:c:i:o:vh")) != -1) {
        switch (opt) {
        case 'a':
            if (strlen(opts.benchmarks) + strlen(optarg) + 2 < sizeof(opts.benchmarks)) {
                if (strlen(opts.benchmarks) > 0) strcat(opts.benchmarks, ",");
                strcat(opts.benchmarks, optarg);
            }
            break;
        case 'c':
            opts.fd_count = atoi(optarg);
            break;
        case 'i':
            opts.iterations = atoi(optarg);
            break;
        case 'o':
            strncpy(opts.output_file, optarg, sizeof(opts.output_file) - 1);
            break;
        case 'v':
            opts.verbose = 1;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    if (strlen(opts.benchmarks) == 0) {
        fprintf(stderr, "Error: At least one benchmark must be specified\n");
        print_usage(argv[0]);
        return 1;
    }

    if (opts.verbose) {
        printf("Syscall Latency Profiler\n");
        printf("Benchmarks: %s\n", opts.benchmarks);
        printf("Iterations: %d\n", opts.iterations);
        printf("Output: %s\n\n", opts.output_file);
    }

    bench_result_t results[10];
    int result_count = 0;

    /* Parse and run benchmarks */
    char *benchmarks_copy = strdup(opts.benchmarks);
    char *saveptr;
    char *bench_name = strtok_r(benchmarks_copy, ",", &saveptr);

    while (bench_name && result_count < 10) {
        histogram_t *hist = histogram_alloc();
        if (!hist) {
            fprintf(stderr, "Failed to allocate histogram\n");
            free(benchmarks_copy);
            return 1;
        }

        struct timespec bench_start, bench_end;
        TIME_NOW(bench_start);

        if (strcmp(bench_name, "epoll") == 0) {
            printf("Running epoll benchmark...\n");
            int fd_counts[] = {10, 100, 1000};
            for (int i = 0; i < 3; i++) {
                if (opts.verbose)
                    printf("  epoll with %d fds\n", fd_counts[i]);
                if (bench_epoll(fd_counts[i], opts.iterations, hist) < 0) {
                    fprintf(stderr, "epoll benchmark failed\n");
                    free(benchmarks_copy);
                    histogram_free(hist);
                    return 1;
                }
            }
            results[result_count].name = "epoll";
        } else if (strcmp(bench_name, "futex") == 0) {
            printf("Running futex benchmark...\n");
            if (bench_futex(opts.iterations, hist) < 0) {
                fprintf(stderr, "futex benchmark failed\n");
                free(benchmarks_copy);
                histogram_free(hist);
                return 1;
            }
            results[result_count].name = "futex";
        } else if (strcmp(bench_name, "io") == 0) {
            printf("Running I/O benchmark...\n");
            if (bench_io(opts.iterations, hist) < 0) {
                fprintf(stderr, "I/O benchmark failed\n");
                free(benchmarks_copy);
                histogram_free(hist);
                return 1;
            }
            results[result_count].name = "io";
        } else {
            fprintf(stderr, "Unknown benchmark: %s\n", bench_name);
            free(benchmarks_copy);
            histogram_free(hist);
            return 1;
        }

        TIME_NOW(bench_end);
        results[result_count].hist = hist;
        results[result_count].total_iterations = hist->sample_count;
        results[result_count].duration_sec =
            (double)(timespec_to_ns(bench_end) - timespec_to_ns(bench_start)) / 1e9;

        if (opts.verbose) {
            histogram_print_summary(hist);
        }

        result_count++;
        bench_name = strtok_r(NULL, ",", &saveptr);
    }

    free(benchmarks_copy);

    /* Write JSON output */
    if (write_json_output(opts.output_file, results, result_count) < 0) {
        fprintf(stderr, "Failed to write output file\n");
        return 1;
    }

    printf("Results written to %s\n", opts.output_file);

    /* Cleanup */
    for (int i = 0; i < result_count; i++) {
        histogram_free(results[i].hist);
    }

    return 0;
}
