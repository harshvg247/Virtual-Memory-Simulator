#ifndef IPC_H
#define IPC_H

/* ipc.h
 *
 * Thin wrapper around System-V shared memory and message queues for
 * the virtual-memory simulator.
 *
 * The API is intentionally small and explicit:
 * - create / attach / detach / remove shared memory
 * - create / send / receive / remove message queues
 *
 * Author: ChatGPT (adapted for your project)
 * Date: 2025-08-22
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <stddef.h>

/* ipc return codes */
#define IPC_OK 0
#define IPC_ERR -1

/* Convenience typedefs */
typedef int ipc_shmid_t;  /* shared memory id */
typedef int ipc_mqid_t;   /* message queue id */

/* Message types we will use across modules (master uses these numbers for msgrcv filters) */
#define MSGTYPE_PROC_REQ     1  /* process -> MMU requests (or generic request) */
#define MSGTYPE_MMU_REPLY    2  /* MMU -> process replies */
#define MSGTYPE_SCHED_NOTIFY 3  /* MMU -> Scheduler notifications */

/* Generic message payload size: change if needed. Keep small to avoid msg limits. */
#define IPC_PAYLOAD_INTS 4

/* SysV message struct wrapper:
 * Note: msgrcv/msgsnd expect first member long mtype.
 * Use the ints array for pid, page_no, frame_no, status etc.
 */
typedef struct {
    long mtype;
    int ints[IPC_PAYLOAD_INTS];
} ipc_msg_t;

/* ---------- Shared memory helpers ---------- */

/* Create (or get if exists) a shared memory segment.
 * key        : System V key (use ftok or integer casted to key_t)
 * size_bytes : number of bytes to allocate
 * shmflg     : permissions + IPC_CREAT flag if needed (e.g., 0666 | IPC_CREAT)
 * Returns shmid on success (>=0), or -1 on failure.
 */
ipc_shmid_t ipc_create_shm(key_t key, size_t size_bytes, int shmflg);

/* Attach an existing shared memory segment to this process's address space.
 * Returns pointer on success, NULL on failure.
 */
void *ipc_attach_shm(ipc_shmid_t shmid);

/* Detach a previously attached shared memory pointer.
 * Returns 0 on success, -1 on failure.
 */
int ipc_detach_shm(void *addr);

/* Remove (mark for deletion) a shared memory segment identified by shmid.
 * Returns 0 on success, -1 on failure.
 */
int ipc_remove_shm(ipc_shmid_t shmid);

/* ---------- Message queue helpers ---------- */

/* Create (or get) a SysV message queue.
 * key   : System V key
 * msgflg: permissions + IPC_CREAT if needed
 * Returns mqid on success (>=0), or -1 on failure.
 */
ipc_mqid_t ipc_create_mq(key_t key, int msgflg);

/* Remove a message queue.
 * Returns 0 on success, -1 on failure.
 */
int ipc_remove_mq(ipc_mqid_t mqid);

/* Send a message over the given queue.
 * Blocks unless IPC_NOWAIT used by caller on msqid? Not exposed here; by default blocks.
 * Returns 0 on success, -1 on failure.
 */
int ipc_send_msg(ipc_mqid_t mqid, const ipc_msg_t *msg);

/* Receive a message (blocking) from the given queue.
 * If 'mtype' is 0 => receive the first message in queue; if >0 => receive the first with that type.
 * Returns number of bytes read (>0) on success, -1 on failure.
 */
ssize_t ipc_recv_msg(ipc_mqid_t mqid, ipc_msg_t *msg, long mtype);

/* Helper: non-blocking receive (IPC_NOWAIT). Returns >0 bytes on success, 0 if no message, -1 on error. */
ssize_t ipc_recv_msg_nb(ipc_mqid_t mqid, ipc_msg_t *msg, long mtype);

/* ---------- Convenience wrappers for the VM simulator ---------- */

/* Create the three message queues used by the lab:
 *  - ready_queue_key => MQ1 (ready queue)
 *  - sched_mmu_key   => MQ2 (scheduler <-> mmu)
 *  - proc_mmu_key    => MQ3 (process <-> mmu)
 *
 * The function returns 0 on success and fills the mqids[] array with mqids.
 */
int ipc_create_vm_mqs(key_t ready_queue_key, key_t sched_mmu_key, key_t proc_mmu_key,
                      ipc_mqid_t mqids[3], int perms);

/* Destroy the three message queues */
int ipc_remove_vm_mqs(ipc_mqid_t mqids[3]);

#endif /* IPC_H */
