CC := gcc
CFLAGS := -O2 -march=native -Wall -Wextra -std=c99 -D_GNU_SOURCE -I./include
LDFLAGS := -lm -pthread
DEPS := $(wildcard include/*.h)
SRCS := src/main.c src/bench_epoll.c src/bench_futex.c src/bench_io.c src/stats.c src/affinity.c
OBJS := $(SRCS:.c=.o)
TARGET := bin/syscall-bench

.PHONY: all clean bench

all: $(TARGET)

$(TARGET): $(OBJS) | bin
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

bin:
	mkdir -p bin

bench: $(TARGET)
	./$(TARGET) -a epoll -a futex -a io -o results.json

clean:
	rm -f $(OBJS)
	rm -rf bin
	rm -f results.json

.PHONY: help
help:
	@echo "Syscall Latency Profiler - Build Targets"
	@echo "  make all        - Build the profiler (default)"
	@echo "  make clean      - Remove build artifacts"
	@echo "  make bench      - Build and run all benchmarks"
	@echo ""
	@echo "Usage examples:"
	@echo "  $(TARGET) -a epoll -o results.json"
	@echo "  $(TARGET) -a futex -c 1000 -o results.json"
	@echo "  $(TARGET) -a io -o results.json"
