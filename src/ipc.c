#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

/* ---------- Shared memory functions ---------- */

// Lifecycle of shared memory
// Create -> Attach -> Use -> Detach -> Remove

ipc_shmid_t ipc_create_shm(key_t key, size_t size_bytes, int shmflg)
{
    if (size_bytes == 0)
    {
        // Writes to standard error (stderr), which is a separate stream meant specifically for error messages
        fprintf(stderr, "ipc_create_shm: size_bytes must be > 0\n");
        return -1;
    }
    ipc_shmid_t shmid = shmget(key, size_bytes, shmflg);
    if (shmid == -1)
    {
        // perror prints a descriptive error message based on the global variable errno (set by system calls when they fail)
        perror("shmget");
        return -1;
    }
    return shmid;
}

// void * is a generic pointer type — it can point to data of any type
// shared memory could hold anything — integers, structs, arrays, etc. The kernel doesn’t know what you’ll store there
// You can later cast it to the type you actually need

void *ipc_attach_shm(ipc_shmid_t shmid)
{
    // shmat(shmid, NULL, 0) attaches a shared memory segment (identified by shmid) into the process’s address space
    void *addr = shmat(shmid, NULL, 0);
    if (addr == (void *)-1)
    {
        perror("shmat");
        return NULL;
    }
    return addr;
}

int ipc_detach_shm(void *addr)
{
    if (shmdt(addr) == -1)
    {
        perror("shmdt");
        return -1;
    }
    return 0;
}

// shmctl is a control function for shared memory

int ipc_remove_shm(ipc_shmid_t shmid)
{
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
    {
        perror("shmctl(IPC_RMID)");
        return -1;
    }
    return 0;
}

/* ---------- Message queue functions ---------- */

ipc_mqid_t ipc_create_mq(key_t key, int msgflg)
{
    // If msgflg includes IPC_CREAT, it will create the queue if it doesn’t exist
    ipc_mqid_t mqid = msgget(key, msgflg);
    if (mqid == -1)
    {
        perror("msgget");
        return -1;
    }
    return mqid;
}

// After marking for deletion, a shared memory segment can still be used by processes already attached until they detach,
// while a message queue becomes unavailable immediately once removed

int ipc_remove_mq(ipc_mqid_t mqid)
{
    if (msgctl(mqid, IPC_RMID, NULL) == -1)
    {
        perror("msgctl(IPC_RMID)");
        return -1;
    }
    return 0;
}

int ipc_send_msg(ipc_mqid_t mqid, const ipc_msg_t *msg)
{
    /* msgsnd requires pointer to payload without the long length omitted.
     * We use sizeof(ipc_msg_t) - sizeof(long) to pass the payload length.
     */
    ssize_t payload_sz = sizeof(ipc_msg_t) - sizeof(long);
    if (msgsnd(mqid, (void *)msg, payload_sz, 0) == -1)
    {
        perror("msgsnd");
        return -1;
    }
    return 0;
}

ssize_t ipc_recv_msg(ipc_mqid_t mqid, ipc_msg_t *msg, long mtype)
{
    ssize_t payload_sz = sizeof(ipc_msg_t) - sizeof(long);
    // mtype parameter controls what to receive:
    // 0 → receive first message in queue.
    // >0 → receive first message of that exact type.
    // <0 → receive first message with type ≤ |mtype|
    ssize_t msg_len = msgrcv(mqid, msg, payload_sz, mtype, 0);
    if (msg_len == -1)
    {
        perror("msgrcv");
        return -1;
    }
    return msg_len;
}

ssize_t ipc_recv_msg_nb(ipc_mqid_t mqid, ipc_msg_t *msg, long mtype)
{
    ssize_t payload_sz = sizeof(ipc_msg_t) - sizeof(long);
    ssize_t msg_len = msgrcv(mqid, msg, payload_sz, mtype, IPC_NOWAIT);
    if (msg_len == -1)
    {
        if (errno == ENOMSG)
        {
            return 0; /* no message available */
        }
        perror("msgrcv(nb)");
        return -1;
    }
    return msg_len;
}

/* ---------- Convenience wrappers for VM simulator ---------- */
// either all three queues are ready, or none exist

int ipc_create_vm_mqs(key_t ready_queue_key, key_t sched_mmu_key, key_t proc_mmu_key, ipc_mqid_t mqids[3], int perms)
{
    if (!mqids)
        return -1;
    int flags = perms | IPC_CREAT;
    ipc_mqid_t mq1 = ipc_create_mq(ready_queue_key, flags);
    if (mq1 == -1)
        return -1;
    ipc_mqid_t mq2 = ipc_create_mq(sched_mmu_key, flags);
    if (mq2 == -1)
    {
        ipc_remove_mq(mq1);
        return -1;
    }
    ipc_mqid_t mq3 = ipc_create_mq(proc_mmu_key, flags);
    if (mq3 == -1)
    {
        ipc_remove_mq(mq1);
        ipc_remove_mq(mq2);
        return -1;
    }

    mqids[0] = mq1;
    mqids[1] = mq2;
    mqids[2] = mq3;
    return 0;
}

int ipc_remove_vm_mqs(ipc_mqid_t mqids[3])
{
    if (!mqids)
        return -1;
    int rc = 0;
    if (ipc_remove_mq(mqids[0]) == -1)
        rc = -1;
    if (ipc_remove_mq(mqids[1]) == -1)
        rc = -1;
    if (ipc_remove_mq(mqids[2]) == -1)
        rc = -1;
    return rc;
}
