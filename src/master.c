/* master.c
 * Master controller for demand-paged VM simulator.
 *
 * Responsibilities:
 *   - Create IPC (SM1, SM2, MQ1, MQ2, MQ3)
 *   - Initialize page tables + frame list
 *   - Generate per-process ref strings + m_req
 *   - Spawn scheduler, mmu, processes
 *   - Wait and cleanup
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <time.h>
#include "ipc.h"
#include "types.h"
#include "master.h"
#include "utils.h"
#include "memory.h"
#include "utils.h"

// #define KEY_SM1 0x1111
// #define KEY_SM2 0x2222
// #define KEY_MQ1 0x3333
// #define KEY_MQ2 0x4444
// #define KEY_MQ3 0x5555

#define LOG(fmt, ...)                                         \
    do                                                        \
    {                                                         \
        fprintf(stdout, "[MASTER] " fmt "\n", ##__VA_ARGS__); \
        fflush(stdout);                                       \
    } while (0)

// adding static makes the function have internal linkage → it is only visible inside the same .c file
// const char *prog → prog points to a string that cannot be modified.
// char *const argv[] → argv itself cannot point elsewhere, but its elements (char *) can be changed

/* Helper: create children via fork/exec */
static pid_t spawn_child(const char *prog, char *const argv[])
{
    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(1);
    }
    if (pid == 0)
    {
        execvp(prog, argv);
        perror("execvp");
        exit(1);
    }
    return pid;
}

key_t KEY_SM1, KEY_SM2, KEY_MQ1, KEY_MQ2, KEY_MQ3;

int init_keys()
{
    const char *path = "./tmp/ftokfile";

    KEY_SM1 = ftok(path, 1);
    KEY_SM2 = ftok(path, 2);
    KEY_MQ1 = ftok(path, 3);
    KEY_MQ2 = ftok(path, 4);
    KEY_MQ3 = ftok(path, 5);
    if (KEY_SM1 == -1 || KEY_SM2 == -1 || KEY_MQ1 == -1 || KEY_MQ2 == -1 || KEY_MQ3 == -1)
    {
        perror("ftok");
        return -1;
    }
    return 0;
}

int master_run(int num_procs, int pgs_per_proc, int n_frms, int ref_len)
{
    srand(time(NULL));
    LOG("Starting master: num_procs=%d pgs_per_proc=%d n_frms=%d ref_len=%d", num_procs, pgs_per_proc, n_frms, ref_len);

    // create keys using ftok
    if (init_keys() == -1)
    {
        perror("init_keys");
        exit(1);
    }

    // create shared memory
    ipc_shmid_t shmid_sm1 = ipc_create_shm(KEY_SM1, sm1_bytes_for_k_m(num_procs, pgs_per_proc), IPC_CREAT | 0666);
    ipc_shmid_t shmid_sm2 = ipc_create_shm(KEY_SM2, sm2_bytes_for_f(n_frms), IPC_CREAT | 0666);
    if (shmid_sm1 == -1 || shmid_sm2 == -1)
    {
        perror("shmget");
        return 1;
    }

    pte_t *sm1_base = (pte_t *)ipc_attach_shm(shmid_sm1);
    free_frame_list_t *ffl = (free_frame_list_t *)ipc_attach_shm(shmid_sm2);

    // initialising
    for (int i = 0; i < num_procs * pgs_per_proc; i++)
    {
        sm1_base[i].valid = -1;
        sm1_base[i].frame_no = -1;
        sm1_base[i].last_used = -1;
    }

    ffl->total_frames = n_frms;
    ffl->count = 0;
    ffl->head = 0;
    ffl->tail = n_frms % n_frms;
    for (int i = 0; i < n_frms; i++)
    {
        ffl->frames[i] = i;
    }

    /* --- Create message queues --- */
    ipc_mqid_t mq1 = ipc_create_mq(KEY_MQ1, IPC_CREAT | 0666);
    ipc_mqid_t mq2 = ipc_create_mq(KEY_MQ2, IPC_CREAT | 0666);
    ipc_mqid_t mq3 = ipc_create_mq(KEY_MQ3, IPC_CREAT | 0666);
    if (mq1 == -1 || mq2 == -1 || mq3 == -1)
    {
        perror("msgget");
        return 1;
    }

    char KEY_SM1_str[20], KEY_SM2_str[20], KEY_MQ1_str[20], KEY_MQ2_str[20], KEY_MQ3_str[20], k_str[20], m_str[20], n_str[20];
    int_to_str((int)KEY_SM1, KEY_SM1_str, sizeof(KEY_SM1_str));
    int_to_str((int)KEY_SM2, KEY_SM2_str, sizeof(KEY_SM2_str));
    int_to_str((int)KEY_MQ1, KEY_MQ1_str, sizeof(KEY_MQ1_str));
    int_to_str((int)KEY_MQ2, KEY_MQ2_str, sizeof(KEY_MQ2_str));
    int_to_str((int)KEY_MQ3, KEY_MQ3_str, sizeof(KEY_MQ3_str));
    int_to_str(num_procs, k_str, sizeof(k_str));
    int_to_str(pgs_per_proc, m_str, sizeof(m_str));
    int_to_str(n_frms, n_str, sizeof(n_str));

    /* --- Spawn MMU --- */
    char *mmu_argv[] = {
        "./mmu",
        KEY_SM1_str, // sm1_key
        KEY_SM2_str, // sm2_key
        KEY_MQ2_str, // mq_sched
        KEY_MQ3_str, // mq_proc
        k_str,
        m_str,
        n_str,
        (char *)NULL};
    spawn_child("./mmu", mmu_argv);

    // spawn scheduler
    char *sched_argv[] = {
        "./scheduler",
        KEY_MQ1_str,
        KEY_MQ2_str,
        k_str,
        NULL};
    spawn_child("./scheduler", sched_argv);

    // spawn processes
    for (int p_ind = 0; p_ind < num_procs; p_ind++)
    {
        // int m_req = 1 + rand() % m;

        // generate reference strings
        int *refs = malloc(ref_len * sizeof(int));
        for (int i = 0; i < ref_len; i++)
        {
            // all legal for now(do +2 for illegal)
            int choice = rand() % (pgs_per_proc); // some may be illegal
            refs[i] = choice;
        }

        char ref_len_str[20], p_ind_str[20];
        int_to_str(ref_len, ref_len_str, sizeof(ref_len_str));
        int_to_str(p_ind, p_ind_str, sizeof(p_ind_str));

        char **proc_argv = malloc((6 + ref_len) * sizeof(char *));
        proc_argv[0] = "./process";
        proc_argv[1] = KEY_MQ1_str;
        proc_argv[2] = KEY_MQ3_str;
        proc_argv[3] = ref_len_str;
        proc_argv[4] = p_ind_str;

        for (int i = 0; i < ref_len; i++)
        {
            char *buf = malloc(20);
            int_to_str(refs[i], buf, sizeof(buf));
            proc_argv[5 + i] = buf;
        }
        proc_argv[5 + ref_len] = NULL;

        LOG("Spawning process: %d", p_ind);

        spawn_child("./process", proc_argv);
        for (int i = 0; i < ref_len; i++)
            free(proc_argv[5 + i]);
        LOG("Cleaning");
        free(proc_argv);
        free(refs);
    }

    while (wait(NULL) > 0)
        ;
    /* --- Cleanup --- */
    LOG("Cleaning up IPC");
    shmctl(shmid_sm1, IPC_RMID, NULL);
    shmctl(shmid_sm2, IPC_RMID, NULL);
    msgctl(mq1, IPC_RMID, NULL);
    msgctl(mq2, IPC_RMID, NULL);
    msgctl(mq3, IPC_RMID, NULL);

    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s <n_procs> <n_pgs_per_proc> <n_frms> <ref_len>\n", argv[0]);
        return 1;
    }
    int num_procs = atoi(argv[1]);
    int pgs_per_proc = atoi(argv[2]);
    int n_frms = atoi(argv[3]);
    int ref_len = atoi(argv[4]);

    return master_run(num_procs, pgs_per_proc, n_frms, ref_len);
}