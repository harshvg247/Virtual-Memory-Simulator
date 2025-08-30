#ifndef MEMORY_H
#define MEMORY_H

/* memory.h
 * Memory subsystem helpers that operate on the shared-memory layouts:
 *  - Initialize page tables in SM1
 *  - Initialize and use Free Frame List (SM2)
 *  - Choose local LRU victim within a single process
 */

#include "types.h"
#include <stddef.h>

/* ---------- Page table init ---------- */

/* Initialize all k page tables (each of size m) in SM1.
 * Sets frame_no = -1, valid = 0, last_used = 0 for every PTE.
 * Returns 0 on success.
 */
int pt_init_all(void *sm1_base, int k, int m);

/* Set a mapping (on page fault resolution): pte[page_no] := (frame_no, valid=1, last_used=ts) */
int pt_set_mapping(void *sm1_base, int pid, int m, int page_no, int frame_no, int ts);

/* Invalidate a page (on eviction): pte[page_no].valid = 0, frame_no = -1 */
int pt_invalidate(void *sm1_base, int pid, int m, int page_no);

/* Touch a page on access: update last_used to 'ts'. (Call this on hits) */
int pt_touch(void *sm1_base, int pid, int m, int page_no, int ts);

/* Validate if a (pid, page_no) pair is within [0, m_req[pid]) — caller passes each process's m_req.
 * Returns 1 if legal, 0 if illegal.
 */
int is_legal_page(int page_no, int m_req_for_pid);

/* ---------- Free Frame List (FFL) management (SM2) ---------- */

/* Compute bytes required for SM2: metadata + f integers. */
static inline size_t sm2_bytes_for_f(int f) {
    return sizeof(free_frame_list_t) + (size_t)f * sizeof(int);
}

/* Initialize FFL on an already-attached SM2 region of size sm2_bytes_for_f(f).
 * Pre-fills frames with 0..f-1 and sets count=f.
 * Returns 0 on success, -1 on bad params.
 */
int ffl_init(free_frame_list_t *ffl, int f);

/* Pop one free frame; returns frame index >=0 on success, or -1 if none available. */
int ffl_alloc(free_frame_list_t *ffl);

/* Push a frame back to free list; returns 0 on success, -1 if full/invalid. */
int ffl_free(free_frame_list_t *ffl, int frame_idx);

/* ---------- Local LRU victim selection ---------- */
/* Choose the least-recently-used VALID page of process pid, scanning its page table.
 * Returns the page_no (>=0) to evict, or -1 if no valid page exists.
 * Complexity: O(m). Good enough for the assignment.
 */
int choose_lru_victim_local(void *sm1_base, int pid, int m);

#endif /* MEMORY_H */
