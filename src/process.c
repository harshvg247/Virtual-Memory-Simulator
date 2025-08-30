/* process.c
 * Simulates a user process generating page references.
 *
 * Steps:
 *  1. Enqueue itself into ready queue (MQ1).
 *  2. Wait until scheduler wakes it up.
 *  3. Iterate over its reference string:
 *      - send request to MMU (MQ3)
 *      - wait for MMU reply
 *      - if hit/fault resolved: continue
 *      - if invalid (-2): terminate
 *  4. At the end: send -9 (end marker) to MMU, then exit.
 */

#define _POSIX_C_SOURCE 200809L // without this 'struct sigaction' is not included in signal.h
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <signal.h>
#include "ipc.h"
#include "types.h"
#include "process.h"

#define LOG(fmt, ...)                                         \
    do                                                        \
    {                                                         \
        fprintf(stdout, "[PROCESS] " fmt "\n", ##__VA_ARGS__); \
        fflush(stdout);                                       \
    } while (0)

// sig_atomic_t is a special integer type defined in <signal.h>.
// It is guaranteed to be read/written atomically
// → meaning operations on it won’t be interrupted mid-way by a signal.

// volatile tells the compiler:
// 👉 "Don’t optimize accesses to this variable; always fetch the latest value from memory."

/* For simplicity: scheduler "wakes" process via SIGCONT */
static volatile sig_atomic_t scheduled = 0;

static void sched_handler(int signo)
{
    (void)signo; // This is a common C trick to silence compiler warnings when you don’t actually use a parameter.
    scheduled = 1;
}

int process_run(int mq_ready_key, int mq_proc_key, int ref_len, int *ref_str, int p_ind)
{   
    int pid = getpid();

    LOG("[process_run()] pid: %d, mq_ready_key: %d, mq_proc_key: %d, ref_len: %d", pid, mq_ready_key, mq_proc_key, ref_len);

    /* Connect to ready queue and proc<->MMU queue */
    ipc_mqid_t mq_ready = ipc_create_mq((key_t)mq_ready_key, 0666);
    if (mq_ready == -1)
    {
        perror("msgget(mq_ready)");
        return 1;
    }
    ipc_mqid_t mq_proc = ipc_create_mq((key_t)mq_proc_key, 0666);
    if (mq_proc == -1)
    {
        perror("msgget(mq_proc)");
        return 1;
    }

    /* Install handler for SIGCONT (scheduler resumes us) */
    struct sigaction sa = {0};
    sa.sa_handler = sched_handler;
    sigaction(SIGCONT, &sa, NULL);

    /* Step 1: register in ready queue */
    ipc_msg_t reg = {0};
    reg.mtype = MSGTYPE_PROC_REQ; /* use proc id as type if you want FCFS fairness */
    reg.ints[0] = pid;
    if (ipc_send_msg(mq_ready, &reg) == -1)
    {
        fprintf(stderr, "Process %d failed to enqueue ready\n", pid);
        return 1;
    }

    /* Step 2: wait until scheduled (pause until SIGCONT) */
    while (!scheduled)
        pause();
    scheduled = 0;
    LOG("Starting process %d", pid);

    /* Step 3: process reference string */
    for (int i = 0; i < ref_len; i++)
    {
        int page_no = ref_str[i];

        // send request to MMU
        ipc_msg_t req = {0};
        req.mtype = MSGTYPE_PROC_REQ;
        req.ints[0] = p_ind;
        req.ints[1] = page_no;
        req.ints[2] = ref_len; /* simplification; ideally pass m_req_for_pid */
        LOG("Sending request");
        ipc_send_msg(mq_proc, &req);

        // wait for reply
        ipc_msg_t reply = {0};
        if (ipc_recv_msg(mq_proc, &reply, MSGTYPE_MMU_REPLY) == -1)
        {
            perror("recv mmu reply");
            break;
        }
        LOG("Received reply");

        int result = reply.ints[1];
        if (result >= 0)
        {
            printf("[Process %d] page=%d -> frame=%d\n", pid, page_no, result);
        }
        else if (result == MMU_INVALID_PAGE)
        {
            printf("[Process %d] INVALID page=%d -> terminating\n", pid, page_no);
            return 0;
        }
    }

    LOG("Finished reference string of process %d\nSending MMU_END_OF_REF", pid);
    
    ipc_msg_t end = {0};
    end.mtype = MSGTYPE_PROC_REQ;
    end.ints[0] = pid;
    end.ints[1] = MMU_END_OF_REF;
    end.ints[2] = -1;   //dummy value
    ipc_send_msg(mq_proc, &end);

    printf("[Process %d] finished reference string\n", pid);
    return 0;
}

/* Standalone binary entry */
int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr,
            "Usage: %s <mq_ready_key> <mq_proc_key> <ref_len> <p_ind> <refs...>\n", argv[0]);
        return 1;
    }
    int mq_ready_key = atoi(argv[1]);
    int mq_proc_key  = atoi(argv[2]);
    int ref_len      = atoi(argv[3]);
    int p_ind        = atoi(argv[4]);

    if (argc < 5 + ref_len) {
        fprintf(stderr, "Not enough references given\n");
        return 1;
    }
    int *refs = malloc(ref_len * sizeof(int));
    for (int i = 0; i < ref_len; i++) {
        refs[i] = atoi(argv[5 + i]);
    }

    int rc = process_run(mq_ready_key, mq_proc_key, ref_len, refs, p_ind);
    free(refs);
    return rc;
}