#ifndef TYPES_H
#define TYPES_H

/* types.h
 * Core data structures for the VM simulator.
 *
 * Shared memory layout (SM1):
 *   We store k page tables back-to-back. Each page table has exactly m entries (virtual pages).
 *   PTE fields: frame_no, valid bit, last_used timestamp.
 *
 * Shared memory layout (SM2):
 *   Free frame list (FFL) holding up to f frame indices and simple queue metadata.
 *
 * NOTE: Only the MMU updates timestamps (global access counter), so page-table
 * writes are serialized through MMU logic.
 */

#include <stddef.h>
#include <stdint.h>

#define MAX_PROCESSES   256   /* sanity cap for tests; not hard-locked */
#define MAX_VPAGES      4096  /* cap on m; adjust as needed */

typedef struct {
    int  frame_no;   /* >=0 if mapped, else -1 */
    int  valid;      /* 1 if in memory, 0 if not */
    int  last_used;  /* global timestamp when last accessed (for LRU) */
} pte_t;

/* Per-process (optional) counters kept in SM1 footer or elsewhere.
 * If you prefer, you can keep these in the master/MMU private memory, not required in SHM.
 */
typedef struct {
    int page_faults;
    int invalid_refs;
} proc_stats_t;

/* Free Frame List (SM2) — single-producer (MMU) model is fine for this simulator. */
typedef struct {
    int total_frames;  /* f */
    int count;         /* number of currently free frames */
    int head;          /* queue head index */
    int tail;          /* queue tail index */
    /* Ring buffer of frame indices [0..total_frames-1]. Size = total_frames. */
    int frames[];      /* flexible array member; allocate with sizeof + f*sizeof(int) */
} free_frame_list_t;

/* ---------- Constants for special MMU returns ---------- */
enum {
    MMU_HIT_MIN      = 0,  /* any non-negative is treated as a frame number */
    MMU_PAGE_FAULT   = -1,
    MMU_INVALID_PAGE = -2,
    MMU_END_OF_REF   = -9
};

// inline → removes function call overhead
// Without static, putting this function in a header file would generate a multiple definition error if included in several .c files
/* ---------- Helpers for working with SM1 layout ---------- */
/* We store k page tables each of length m, contiguous:
 * SM1 size in bytes = k * m * sizeof(pte_t)
 */
static inline size_t sm1_bytes_for_k_m(int k, int m) {
    return (size_t)k * (size_t)m * sizeof(pte_t);
}

/* Compute pointer to the start of process 'pid' page table inside SM1 (0 <= pid < k) */
static inline pte_t* pt_base_for_pid(void *sm1_base, int pid, int m) {
    return (pte_t*)sm1_base + ((size_t)pid * (size_t)m);
}

/* Bounds checking helper (callers should guard in debug builds) */
static inline pte_t* pte_addr(void *sm1_base, int pid, int m, int page_no) {
    return pt_base_for_pid(sm1_base, pid, m) + page_no;
}

#endif /* TYPES_H */
