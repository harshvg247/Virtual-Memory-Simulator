# Virtual Memory Simulator (VMS)

This project simulates a demand-paged virtual memory system. It demonstrates the key components of an operating system's memory subsystem using inter-process communication (IPC) mechanisms, process scheduling, and a memory management unit (MMU).

## Features

- **Master Controller**: Initializes shared memory and message queues, spawns processes, and generates page reference strings.
- **Process Simulation**: Each process simulates generating page references and communicates with the MMU.
- **Memory Management Unit (MMU)**: Handles page faults, validates page requests, and manages address translation.
- **Scheduler**: Resumes processes based on a scheduling policy (using SIGCONT for simplicity).
- **IPC Utilities**: Wraps System V message queues and shared memory operations.

## Project Structure

```
.
├── Makefile               # Build system for the project
├── src/                   # Source code
│   ├── master.c           # Master controller
│   ├── mmu.c              # Memory Management Unit
│   ├── sched.c            # Scheduler
│   ├── process.c          # Process simulation (detailed below)
│   ├── ipc.c              # IPC message queue/shared memory utilities
│   ├── utils.c            # Utility functions
│   ├── memory.c           # Memory subsystem helpers
│   └── include/           # Header files
│       ├── ipc.h
│       ├── master.h
│       ├── memory.h
│       ├── mmu.h
│       ├── process.h
│       ├── scheduler.h
│       ├── types.h
│       └── utils.h
├── tools/                 # Test utilities for IPC and memory modules
│   ├── ipc_test.c         # Test for IPC functionality
│   └── memory_test.c      # Test for memory subsystem
├── tmp/                   # Temporary files (e.g., key files for IPC)
└── README.md              # Project documentation
```

## Process Module Overview

The process simulation (`src/process.c`) performs the following steps:

1. **Queue Registration**: The process registers itself to the ready queue.
2. **Scheduling**: It waits to be resumed by the scheduler (using `SIGCONT`).
3. **Reference Processing**:
   - Iterates through its page reference string.
   - For each page request, the process sends an IPC message to the MMU.
   - Waits for the MMU's reply. If the reply indicates a valid frame mapping, the mapping is printed. If the reply signals an invalid page (e.g., `MMU_INVALID_PAGE`), the process terminates.
4. **End of Reference**: After processing its reference string, the process sends a termination message (`MMU_END_OF_REF`) to the MMU and then exits.

## Build Instructions

The project is built using the provided `Makefile`. Common targets include:

- **Build all components**:
  ```bash
  make
  ```

- **Clean build artifacts**:
  ```bash
  make clean
  ```

## Running the Simulation

### Master
Start the simulation by running the master binary:
```bash
./master <num_procs> <pgs_per_proc> <num_frames> <ref_len>
```
- `num_procs`: Number of processes to simulate.
- `pgs_per_proc`: Maximum number of virtual pages per process.
- `num_frames`: Number of physical frames available.
- `ref_len`: Length of the page reference string for each process.

### Scheduler
Resume processes by running:
```bash
./scheduler <mq_ready_key> <mq_sched_key> <num_procs>
```

### MMU
Start the MMU with:
```bash
./mmu <sm1_key> <sm2_key> <mq_sched_key> <mq_proc_key> <k> <m> <f>
```

### Process
Processes are spawned by the master. They receive their parameters and page references on the command line:
```bash
./process <mq_ready_key> <mq_proc_key> <ref_len> <p_ind> <refs...>
```
- `<ref_len>`: Length of the reference string.
- `<p_ind>`: Process index identifier.
- `<refs...>`: Space-separated list of page references.

## Tests

### IPC Test
Build and run the IPC test to validate the creation and handling of message queues:
```bash
gcc -Wall -g -I./src/include tools/ipc_test.c src/ipc.c -o ipc_test
./ipc_test ./tmp/ftokfile
```

### Memory Test
Test the memory subsystem functionality:
```bash
gcc -Wall -g -I./src/include tools/memory_test.c src/memory.c -o memory_test
./memory_test
```

## License

This project is for educational purposes and is not licensed for commercial use.

## Author

Adapted for your project by GitHub Copilot.
