/* memory_test.c
 * Sanity checks for SM1 layout, page-table init, FFL, and local LRU.
 *
 * Build:
 *   gcc -Wall -g -I./src/include tools/memory_test.c src/memory.c -o memory_test
 *
 * Run:
 *   ./memory_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "memory.h"

int main(void)
{
    int k = 3; /* 3 processes */
    int m = 8; /* 8 virtual pages per process */
    int f = 6; /* 6 physical frames total */

    /* Allocate SM1 in heap for test (real sim uses shm). */
    size_t sm1sz = sm1_bytes_for_k_m(k, m);
    void *sm1 = malloc(sm1sz);
    if (!sm1)
    {
        perror("malloc sm1");
        return 1;
    }

    if (pt_init_all(sm1, k, m) != 0)
    {
        fprintf(stderr, "pt_init_all failed\n");
        return 1;
    }

    /* Create a fake FFL in heap (real sim uses shm). */
    size_t sm2sz = sm2_bytes_for_f(f);
    free_frame_list_t *ffl = (free_frame_list_t *)malloc(sm2sz);
    if (!ffl)
    {
        perror("malloc sm2");
        return 1;
    }

    if (ffl_init(ffl, f) != 0)
    {
        fprintf(stderr, "ffl_init failed\n");
        return 1;
    }

    printf("FFL initialized: total=%d count=%d head=%d tail=%d\n",
           ffl->total_frames, ffl->count, ffl->head, ffl->tail);

    /* Simulate 2 faults for pid=1: load pages 3,5 with frames from FFL; touch them. */
    int ts = 0;
    int pid = 1;

    int fr1 = ffl_alloc(ffl);
    int fr2 = ffl_alloc(ffl);
    printf("Allocated frames: %d, %d; remaining=%d\n", fr1, fr2, ffl->count);

    pt_set_mapping(sm1, pid, m, 3, fr1, ++ts);
    pt_set_mapping(sm1, pid, m, 5, fr2, ++ts);

    /* Access page 3 again (more recent). */
    pt_touch(sm1, pid, m, 3, ++ts);

    /* Now the LRU victim between {3,5} should be 5. */
    int victim = choose_lru_victim_local(sm1, pid, m);
    printf("LRU victim for pid=%d is page=%d (expected 5)\n", pid, victim);

    /* Evict it: free its frame, invalidate PTE. */
    if (victim >= 0)
    {
        int frame_to_free = pte_addr(sm1, pid, m, victim)->frame_no;
        pt_invalidate(sm1, pid, m, victim);
        ffl_free(ffl, frame_to_free);
        printf("Evicted page=%d, freed frame=%d, FFL count=%d\n",
               victim, frame_to_free, ffl->count);
    }

    free(ffl);
    free(sm1);
    return 0;
}
