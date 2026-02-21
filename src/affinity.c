#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <dirent.h>
#include "../include/bench.h"

int cpu_pin_current(int cpu_id) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu_id, &set);

    if (sched_setaffinity(0, sizeof(cpu_set_t), &set) < 0) {
        perror("sched_setaffinity");
        return -1;
    }

    return 0;
}

int cpu_get_nprocs(void) {
    int nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    if (nprocs < 0) {
        perror("sysconf");
        return -1;
    }
    return nprocs;
}

/* Detect NUMA node for given CPU */
int numa_get_node_for_cpu(int cpu_id) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/node%d", cpu_id, cpu_id);

    DIR *dir = opendir(path);
    if (dir) {
        closedir(dir);
        /* Parse node number from path */
        for (int node = 0; node < 16; node++) {
            snprintf(path, sizeof(path), "/sys/devices/system/node/node%d", node);
            dir = opendir(path);
            if (!dir) continue;

            /* Check if this node has cpu_id */
            snprintf(path, sizeof(path), "/sys/devices/system/node/node%d/cpu%d", node, cpu_id);
            if (access(path, F_OK) == 0) {
                closedir(dir);
                return node;
            }
            closedir(dir);
        }
    }

    /* Default: node 0 */
    return 0;
}

/* Get CPUs in given NUMA node */
int numa_get_cpus_for_node(int node_id, int *cpus, int max_cpus) {
    char path[256];
    char cpulist[256];
    int count = 0;

    snprintf(path, sizeof(path), "/sys/devices/system/node/node%d/cpulist", node_id);
    FILE *f = fopen(path, "r");
    if (!f) {
        /* Fallback: query all CPUs */
        int nprocs = cpu_get_nprocs();
        if (nprocs < 0) return -1;

        for (int i = 0; i < nprocs && count < max_cpus; i++) {
            cpus[count++] = i;
        }
        return count;
    }

    if (fgets(cpulist, sizeof(cpulist), f) == NULL) {
        fclose(f);
        return -1;
    }
    fclose(f);

    /* Parse CPU list (format: "0-3,5,7-9") */
    char *saveptr;
    char *range = strtok_r(cpulist, ",", &saveptr);

    while (range && count < max_cpus) {
        char *dash = strchr(range, '-');
        if (dash) {
            int start = atoi(range);
            int end = atoi(dash + 1);
            for (int i = start; i <= end && count < max_cpus; i++) {
                cpus[count++] = i;
            }
        } else {
            cpus[count++] = atoi(range);
        }
        range = strtok_r(NULL, ",", &saveptr);
    }

    return count;
}
