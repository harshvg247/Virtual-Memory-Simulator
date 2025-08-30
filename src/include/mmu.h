#ifndef MMU_H
#define MMU_H

/* mmu.h
 * Public API and CLI contract for the MMU module.
 *
 * CLI (recommended):
 *   mmu <sm1_key> <sm2_key> <mq_sched_key> <mq_proc_key> <k> <m> <f>
 *
 * Where:
 *   sm1_key       : key_t for SM1 (page tables), ftok-derived (pass as int)
 *   sm2_key       : key_t for SM2 (free frame list), ftok-derived (pass as int)
 *   mq_sched_key  : key_t for MQ2 (MMU <-> Scheduler)
 *   mq_proc_key   : key_t for MQ3 (MMU <-> Processes)
 *   k             : number of processes (0..k-1)
 *   m             : virtual pages per process (page table length)
 *   f             : number of physical frames
 *
 * Message protocol (SysV queues; see ipc.h):
 *   - Process -> MMU (MQ3, mtype=MSGTYPE_PROC_REQ):
 *       msg.ints[0] = pid
 *       msg.ints[1] = page_no
 *       msg.ints[2] = m_req_for_pid  (the legal upper bound for this pid)
 *   - MMU -> Process (MQ3, mtype=MSGTYPE_MMU_REPLY):
 *       msg.ints[0] = pid
 *       msg.ints[1] = result
 *           >=0  : frame number (hit or fault resolved)
 *           -2   : MMU_INVALID_PAGE (illegal reference)
 *           -9   : MMU_END_OF_REF (if you choose to send an end marker)
 *   - MMU -> Scheduler (MQ2, mtype=MSGTYPE_SCHED_NOTIFY):
 *       msg.ints[0] = pid
 *       msg.ints[1] = 1 if page fault handled, 0 otherwise
 *
 * The MMU maintains a global timestamp that increments on every *valid* access.
 */

int mmu_run(int sm1_key, int sm2_key,
            int mq_sched_key, int mq_proc_key,
            int k, int m, int f);

#endif /* MMU_H */
