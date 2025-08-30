/* sched.c
 * Simple FCFS scheduler.
 *
 * Flow:
 *   1. Wait for processes to register in ready queue (MQ1).
 *   2. Pick next PID in FIFO order.
 *   3. Send SIGCONT to that process.
 *   4. Listen for MMU notifications on MQ2 until that process ends.
 *   5. Repeat until all processes are done.
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <errno.h>
#include "ipc.h"
#include "types.h"
#include "scheduler.h"

#define LOG(fmt, ...)                                        \
    do                                                       \
    {                                                        \
        fprintf(stdout, "[SCHED] " fmt "\n", ##__VA_ARGS__); \
        fflush(stdout);                                      \
    } while (0)

/* Track finished processes */
static int finished_count = 0;

int scheduler_run(int mq_ready_key, int mq_sched_key, int num_procs)
{
    ipc_mqid_t mq_ready = ipc_create_mq((key_t)mq_ready_key, 0666);
    if (mq_ready == -1)
    {
        perror("msgget(mq_ready)");
        return 1;
    }

    ipc_mqid_t mq_sched = ipc_create_mq((key_t)mq_sched_key, 0666);
    if (mq_sched == -1)
    {
        perror("masget(mq_sched)");
        return 1;
    }

    LOG("Scheduler started (FCFS)");

    while (finished_count < num_procs)
    {
        /* Step 1: dequeue next process from ready queue */
        ipc_msg_t reg = {0};
        if (ipc_recv_msg(mq_ready, &reg, MSGTYPE_PROC_REQ) == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("recv ready");
            break;
        }
        int pid = reg.ints[0];
        LOG("Picked process %d from ready queue", pid);
        sleep(2);
        /* Step 2: send SIGCONT to start/resume the process */
        if (kill(pid, SIGCONT) == -1)
        {
            perror("kill(SIGCONT)");
            continue;
        }

        for (;;)
        {
            ipc_msg_t note = {0};
            if (ipc_recv_msg(mq_sched, &note, MSGTYPE_SCHED_NOTIFY) == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                perror("recv sched");
                break;
            }

            int from_pid = note.ints[0];
            int pfh = note.ints[1];

            if (pfh)
            {
                LOG("Process %d: page fault handled", from_pid);
            }

            /* End detection convention:
             * If MMU sends ints[1] = 0 and we already saw end marker from proc,
             * we can mark it finished.
             */
            if (pfh == 0)
            {
                LOG("Process %d finished", pid);
                finished_count++;
                break;
            }
        }
    }
    LOG("All %d processes finished, scheduler exiting", num_procs);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <mq_ready_key> <mq_sched_key> <num_procs>\n", argv[0]);
        return 1;
    }
    int mq_ready_key = atoi(argv[1]);
    int mq_sched_key = atoi(argv[2]);
    int num_procs = atoi(argv[3]);
    return scheduler_run(mq_ready_key, mq_sched_key, num_procs);
}