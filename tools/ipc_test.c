/*
 * ipc_test.c
 *
 * Small test demonstrating creation + attach + cleanup of the
 * shared memory segments and message queues required by the lab.
 *
 * Usage:
 *   gcc -Wall -g -I./src/include tools/ipc_test.c src/ipc.c -o ipc_test
 *   ./ipc_test ./tmpfile_for_ftok
 *
 * The program:
 *  - creates an ftok-based key file (path provided)
 *  - creates 2 shared memory segments (SM1, SM2)
 *  - creates 3 message queues (MQ1, MQ2, MQ3)
 *  - prints IDs and sizes
 *  - detaches and removes them before exit
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <string.h>
#include "ipc.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <path-for-ftok>\n", argv[0]);
        return 1;
    }

    const char *ftok_path = argv[1];

    /* ensure file exists for ftok */
    // O_CREAT → create the file if it doesn’t already exist.
    // O_RDWR → open for both reading and writing
    int fd = open(ftok_path, O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
        perror("open ftok file");
        return 1;
    }
    close(fd);

    // ftok looks at the inode number and device number of the file at pathname.
    // Combines those with proj_id into a key_t.
    // That means:
    //      If two processes call ftok with the same file path and same proj_id, they get the same key.
    //      Different proj_ids (like 'A', 'B', …) give different keys, even with the same file path.

    /* generate keys via ftok with different project ids */
    key_t key_sm1 = ftok(ftok_path, 'A'); /* page tables */
    key_t key_sm2 = ftok(ftok_path, 'B'); /* free frame list */
    key_t key_mq1 = ftok(ftok_path, 'C'); /* ready queue */
    key_t key_mq2 = ftok(ftok_path, 'D'); /* sched<->mmu */
    key_t key_mq3 = ftok(ftok_path, 'E'); /* proc<->mmu */

    if (key_sm1 == -1 || key_sm2 == -1 || key_mq1 == -1 || key_mq2 == -1 || key_mq3 == -1)
    {
        perror("ftok");
        return 1;
    }

    printf("Keys: SM1=%d, SM2=%d, MQ1=%d MQ2=%d MQ3=%d\n",
           (int)key_sm1, (int)key_sm2, (int)key_mq1, (int)key_mq2, (int)key_mq3);

    /* create shared memory segments (example sizes, change to fit your structs) */
    size_t sm1_size = 1024 * 16; /* e.g., enough for several page tables */
    size_t sm2_size = 1024 * 4;  /* e.g., free frame array */

    ipc_shmid_t shmid1 = ipc_create_shm(key_sm1, sm1_size, 0666 | IPC_CREAT);
    ipc_shmid_t shmid2 = ipc_create_shm(key_sm2, sm2_size, 0666 | IPC_CREAT);

    if (shmid1 == -1 || shmid2 == -1)
    {
        fprintf(stderr, "Failed to create shared memory\n");
        return 1;
    }

    printf("Created SHM: shmid1=%d, shmid2=%d\n", shmid1, shmid2);

    void *addr1 = ipc_attach_shm(shmid1);
    void *addr2 = ipc_attach_shm(shmid2);
    if (!addr1 || !addr2)
    {
        fprintf(stderr, "attach failed\n");
        ipc_remove_shm(shmid1);
        ipc_remove_shm(shmid2);
        return 1;
    }

    printf("Attached SHM at %p and %p\n", addr1, addr2);

    /* create message queues */
    ipc_mqid_t mqids[3];
    if (ipc_create_vm_mqs(key_mq1, key_mq2, key_mq3, mqids, 0666) == -1)
    {
        fprintf(stderr, "msg queue creation failed\n");
        ipc_detach_shm(addr1);
        ipc_detach_shm(addr2);
        ipc_remove_shm(shmid1);
        ipc_remove_shm(shmid2);
        return 1;
    }
    printf("Created MQs: mq1=%d mq2=%d mq3=%d\n", mqids[0], mqids[1], mqids[2]);

    /* Demonstrate sending a message on MQ3 (proc->mmu) */
    ipc_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.mtype = MSGTYPE_PROC_REQ;
    msg.ints[0] = getpid(); /* pid */
    msg.ints[1] = 5;        /* page_no example */

    if (ipc_send_msg(mqids[2], &msg) == 0)
    {
        printf("Sent test request to MQ3\n");
    }
    else
    {
        fprintf(stderr, "Failed to send test message\n");
    }

    /* Receive it (non-blocking) from same queue as demonstration */
    ipc_msg_t rcv;
    ssize_t r = ipc_recv_msg_nb(mqids[2], &rcv, 0);
    if (r > 0)
    {
        printf("Received message mtype=%ld pid=%d page_no=%d\n",
               rcv.mtype, rcv.ints[0], rcv.ints[1]);
    }
    else if (r == 0)
    {
        printf("No message to receive (nb)\n");
    }
    else
    {
        printf("ipc_recv_msg_nb error\n");
    }

    /* cleanup */
    ipc_remove_vm_mqs(mqids);
    ipc_detach_shm(addr1);
    ipc_detach_shm(addr2);
    ipc_remove_shm(shmid1);
    ipc_remove_shm(shmid2);

    printf("Cleaned up IPC resources\n");
    return 0;
}
