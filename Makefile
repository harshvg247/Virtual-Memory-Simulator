# # Compiler to use
# CC = gcc

# # Compiler flags:
# # -Wall → enable all warnings
# # -g    → include debugging information (for gdb/valgrind)
# # -I./src/include → add "src/include" directory to search path for header files
# CFLAGS = -Wall -g -I./src/include

# # Source files for the project
# # SRCS = src/master.c src/sched.c src/mmu.c src/process.c src/memory.c src/ipc.c src/utils.c
# SRCS = src/ipc.c


# # Object files (replace .c with .o)
# OBJS = $(SRCS:.c=.o)

# # Default target (runs when you type `make`)
# all: VMS

# # Link step: build the final executable "VMS" from object files
# # -lrt links against the "real-time" library (needed for POSIX shared memory, message queues, etc.)
# VMS: $(OBJS)
# 	$(CC) $(CFLAGS) -o VMS $(OBJS) -lrt

# # Clean target: removes object files and the binary
# # Run this with: make clean
# clean:
# 	rm -f $(OBJS) VMS

#ipc_test
# CC = gcc
# CFLAGS = -Wall -g -I./src/include    # if you #include "ipc.h", compiler will look inside ./src/include/ipc.h
# SRC = src/ipc.c
# TEST = tools/ipc_test.c

# all: ipc_test

# # # -o ipc_test → output binary named ipc_test.
# # It links ipc.c and ipc_test.c into one executable

# ipc_test: $(SRC) $(TEST)
# 	$(CC) $(CFLAGS) -o ipc_test $(SRC) $(TEST)   

#memory_test
# CC=gcc
# CFLAGS=-Wall -g -I./src/include

# MEM_SRCS=src/memory.c
# IPC_SRCS=src/ipc.c

# tests: memory_test

# memory_test: $(MEM_SRCS) tools/memory_test.c
# 	$(CC) $(CFLAGS) -o memory_test $(MEM_SRCS) tools/memory_test.c

#complete

CC = gcc
CFLAGS = -Wall -Wextra -g -I./src/include

SRCS = src/master.c src/mmu.c src/sched.c src/process.c src/ipc.c src/utils.c
OBJS = $(SRCS:.c=.o)

all: master mmu scheduler process

master: src/master.o src/ipc.o src/utils.o
	$(CC) $(CFLAGS) -o master src/master.o src/ipc.o src/utils.o

mmu: src/mmu.o src/ipc.o src/memory.o
	$(CC) $(CFLAGS) -o mmu src/mmu.o src/ipc.o src/memory.o

scheduler: src/sched.o src/ipc.o
	$(CC) $(CFLAGS) -o scheduler src/sched.o src/ipc.o

process: src/process.o src/ipc.o
	$(CC) $(CFLAGS) -o process src/process.o src/ipc.o

clean:
	rm -f src/*.o master mmu scheduler process
