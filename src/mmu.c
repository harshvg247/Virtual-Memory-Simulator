/* mmu.c
 * Demand-paged MMU with local LRU replacement.
 *
 * Responsibilities:
 *  - Attach to SM1 (page tables) and SM2 (free frame list)
 *  - Handle proc->MMU requests on MQ3:
 *      * Illegal page -> reply INVALID
 *      * Hit          -> touch (LRU), reply frame
 *      * Fault        -> allocate or evict (local LRU), map, reply frame
 *  - Notify scheduler on MQ2 when a page fault occurs (optional but useful)
 *
 * Build:
 *   gcc -Wall -g -I./src/include src/mmu.c src/memory.c src/ipc.c -o mmu -lrt
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>

#include "types.h"
#include "memory.h"
#include "ipc.h"
#include "mmu.h"

/* Global timestamp increases per *valid* access. */
static int g_ts = 0;

/* Logging macro (stdout for now) */
#define LOG(fmt, ...)                                      \
    do                                                     \
    {                                                      \
        fprintf(stdout, "[MMU] " fmt "\n", ##__VA_ARGS__); \
        fflush(stdout);                                    \
    } while (0)

/* Helper: send reply to a process on MQ3 */
static int send_proc_reply(ipc_mqid_t mq_proc, int pid, int result)
{
    ipc_msg_t reply = {0};
    reply.mtype = MSGTYPE_MMU_REPLY;
    reply.ints[0] = pid;
    reply.ints[1] = result;
    return ipc_send_msg(mq_proc, &reply);
}

/* Helper: notify scheduler (e.g., that a page fault was handled) on MQ2 */
static int notify_scheduler(ipc_mqid_t mq_sched, int pid, int pfh_handled)
{
    ipc_msg_t note = {0};
    note.mtype = MSGTYPE_SCHED_NOTIFY;
    note.ints[0] = pid;
    note.ints[1] = pfh_handled ? 1 : 0;
    return ipc_send_msg(mq_sched, &note);
}

static int resolve_access(void *sm1_base, free_frame_list_t *ffl, int p_ind,
                          int page_no, int m, int m_req_for_pid, int *pfh_out)
{
    *pfh_out = 0;
    if (!is_legal_page(page_no, m_req_for_pid))
    {
        LOG("p_ind=%d illegal page=%d (limit=%d)", p_ind, page_no, m_req_for_pid);
        return MMU_INVALID_PAGE;
    }
    // LOG("calling pte_addr");
    pte_t *pte = pte_addr(sm1_base, p_ind, m, page_no);
    // LOG("checking validity");
    if (pte->valid > 0)
    {
        // LOG("pte is valid");
        /* HIT: update LRU timestamp and return frame */
        pte->last_used = ++g_ts;
        LOG("p_ind=%d hit page=%d -> frame=%d (ts=%d)", p_ind, page_no, pte->frame_no, g_ts);
        return pte->frame_no;
    }
    // LOG("calling ffl_alloc()");
    /* FAULT: try to allocate a free frame */
    int frame = ffl_alloc(ffl);
    // LOG("%d", frame);
    if (frame >= 0)
    {
        pt_set_mapping(sm1_base, p_ind, m, page_no, frame, ++g_ts);
        *pfh_out = 1;
        LOG("p_ind=%d fault page=%d allocated frame=%d (ts=%d)", p_ind, page_no, frame, g_ts);
        return frame;
    }

    /* No free frame: evict local LRU victim from THIS pid only */
    int victim_page = choose_lru_victim_local(sm1_base, p_ind, m);
    if (victim_page < 0)
    {
        /* If a process has no valid pages yet but FFL is empty, the system is overcommitted.
           For this assignment, just report fault cannot be handled (rare) — or pick a global victim.
           We'll print and fail the access. */
        LOG("p_ind=%d cannot handle fault (no free frame, no local victim). Consider global policy.", p_ind);
        return MMU_PAGE_FAULT; /* unreachable in our reply protocol; caller can handle if desired */
    }

    int victim_frame = pte_addr(sm1_base, p_ind, m, victim_page)->frame_no;
    pt_invalidate(sm1_base, p_ind, m, victim_page);

    pt_set_mapping(sm1_base, p_ind, m, page_no, victim_frame, ++g_ts);
    *pfh_out = 1;
    LOG("p_ind=%d fault page=%d evicted page=%d -> frame=%d (ts=%d)",
        p_ind, page_no, victim_page, victim_frame, g_ts);
    return victim_frame;
}

int mmu_run(int sm1_key, int sm2_key, int mq_sched_key, int mq_proc_key,
            int k, int m, int f)
{
    // Attach shared memory segment(No creation)
    ipc_shmid_t shmid_sm1 = shmget((key_t)sm1_key, sm1_bytes_for_k_m(k, m), 0666);

    static int procs_cmpltd = 0;

    if (shmid_sm1 == -1)
    {
        perror("shmget(SM1)");
        return 1;
    }
    void *sm1_base = ipc_attach_shm(shmid_sm1);
    if (!sm1_base)
        return 1;

    ipc_shmid_t shmid_sm2 = shmget((key_t)sm2_key, sm2_bytes_for_f(f), 0666);
    if (shmid_sm2 == -1)
    {
        perror("shmget(SM2)");
        ipc_detach_shm(sm1_base); // not remove
        return 1;
    }
    free_frame_list_t *ffl = (free_frame_list_t *)ipc_attach_shm(shmid_sm2);
    if (!ffl)
    {
        ipc_detach_shm(sm1_base);
        return 1;
    }

    /* Open message queues (not create)*/
    ipc_mqid_t mq_sched = ipc_create_mq((key_t)mq_sched_key, 0666);
    if (mq_sched == -1)
    {
        ipc_detach_shm(sm1_base);
        ipc_detach_shm(ffl);
        return 1;
    }

    ipc_mqid_t mq_proc = ipc_create_mq((key_t)mq_proc_key, 0666);
    if (mq_proc == -1)
    {
        ipc_detach_shm(sm1_base);
        ipc_detach_shm(ffl);
        return 1;
    }

    LOG("MMU started: k=%d m=%d f=%d", k, m, f);

    while (1)
    {
        ipc_msg_t req = {0};
        ssize_t r = ipc_recv_msg(mq_proc, &req, MSGTYPE_PROC_REQ);
        sleep(3);
        // LOG("Received msg");
        if (r < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("msgrcv(proc->mmu)");
            break;
        }

        int p_ind = req.ints[0];
        int page_no = req.ints[1];
        int m_req_for_pid = req.ints[2];

        /* Optional end-of-stream convention: pid sends page_no = -9 to indicate done */
        if (page_no == MMU_END_OF_REF)
        {
            LOG("pid=%d end-of-ref", p_ind);
            send_proc_reply(mq_proc, p_ind, MMU_END_OF_REF);

            /* Notify scheduler */
            notify_scheduler(mq_sched, p_ind, 0);
            procs_cmpltd++;
            if(procs_cmpltd >= k){
                break;
            }
            continue;
        }
        int pfh = 0;
        // LOG("Resolvong access");
        int result = resolve_access(sm1_base, ffl, p_ind, page_no, m, m_req_for_pid, &pfh);
        // LOG("result acquired");
        send_proc_reply(mq_proc, p_ind, result);
        // LOG("reply sent");
        if (pfh)
        {
            notify_scheduler(mq_sched, p_ind, 1);
        }
    }

    LOG("Shutting down MMU...");

    ipc_detach_shm(sm1_base);
    ipc_detach_shm(ffl);
    return 0;
}

/* Standalone binary entrypoint (optional but handy). */
int main(int argc, char **argv)
{
    if (argc != 8)
    {
        fprintf(stderr,
                "Usage: %s <sm1_key> <sm2_key> <mq_sched_key> <mq_proc_key> <k> <m> <f>\n", argv[0]);
        return 1;
    }
    int sm1_key = atoi(argv[1]);
    int sm2_key = atoi(argv[2]);
    int mq_sched_key = atoi(argv[3]);
    int mq_proc_key = atoi(argv[4]);
    int k = atoi(argv[5]);
    int m = atoi(argv[6]);
    int f = atoi(argv[7]);

    return mmu_run(sm1_key, sm2_key, mq_sched_key, mq_proc_key, k, m, f);
}
