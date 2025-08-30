/* memory.c
 * Implementation of page-table initialization, free-frame list, and local LRU selection.
 */

#include "memory.h"
#include <stdio.h>
#include "types.h"

/* ---------- Page table ops ---------- */

int pt_init_all(void *sm1_base, int k, int m)
{
    if (!sm1_base || k <= 0 || m <= 0)
        return -1;
    pte_t *base = (pte_t *)sm1_base;
    size_t total = (size_t)k * (size_t)m;
    for (size_t i = 0; i < total; ++i)
    {
        base[i].frame_no = -1;
        base[i].valid = 0;
        base[i].last_used = 0;
    }
    return 0;
}

int pt_set_mapping(void *sm1_base, int pid, int m, int page_no, int frame_no, int ts)
{
    if (!sm1_base || pid < 0 || m <= 0 || page_no < 0 || page_no >= m || frame_no < 0)
        return -1;
    pte_t *pte = pte_addr(sm1_base, pid, m, page_no);
    pte->frame_no = frame_no;
    pte->valid = 1;
    pte->last_used = ts;
    return 0;
}

int pt_invalidate(void *sm1_base, int pid, int m, int page_no)
{
    if (!sm1_base || pid < 0 || m <= 0 || page_no < 0 || page_no >= m)
        return -1;
    pte_t *pte = pte_addr(sm1_base, pid, m, page_no);
    pte->frame_no = -1;
    pte->valid = 0;
    /* keep last_used as-is (optional) */
    return 0;
}

int pt_touch(void *sm1_base, int pid, int m, int page_no, int ts) {
    if (!sm1_base || pid < 0 || m <= 0 || page_no < 0 || page_no >= m) return -1;
    pte_t *pte = pte_addr(sm1_base, pid, m, page_no);
    if (!pte->valid) return -1;
    pte->last_used = ts;
    return 0;
}

int is_legal_page(int page_no, int m_req_for_pid) {
    return (page_no >= 0 && page_no < m_req_for_pid) ? 1 : 0;
}

/* ---------- Free Frame List (FFL) ---------- */

int ffl_init(free_frame_list_t *ffl, int f) {
    if (!ffl || f <= 0) return -1;
    ffl->total_frames = f;
    ffl->count = f;
    ffl->head = 0;
    ffl->tail = 0;
    for (int i = 0; i < f; ++i) {
        ffl->frames[i] = i;
    }
    /* tail points to the next write position; since we prefilled with f entries,
       we advance tail by f in ring terms */
    ffl->tail = f % f; /* which is 0, but keep formula if you adapt later */
    return 0;
}

int ffl_alloc(free_frame_list_t *ffl) {
    if (!ffl || ffl->count > ffl->total_frames) return -1;
    int idx = ffl->frames[ffl->head];
    ffl->head = (ffl->head + 1) % ffl->total_frames;
    ffl->count--;
    return idx;
}

int ffl_free(free_frame_list_t *ffl, int frame_idx) {
    if (!ffl || frame_idx < 0 || frame_idx >= ffl->total_frames) return -1;
    if (ffl->count >= ffl->total_frames) return -1; /* full */
    ffl->frames[ffl->tail] = frame_idx;
    ffl->tail = (ffl->tail + 1) % ffl->total_frames;
    ffl->count++;
    return 0;
}

int choose_lru_victim_local(void *sm1_base, int pid, int m) {
    if (!sm1_base || pid < 0 || m <= 0) return -1;
    pte_t *pt = pt_base_for_pid(sm1_base, pid, m);
    int victim = -1;
    int oldest_ts = 0; /* will be set on first valid */
    for (int p = 0; p < m; ++p) {
        if (pt[p].valid) {
            if (victim == -1 || pt[p].last_used < oldest_ts) {
                victim = p;
                oldest_ts = pt[p].last_used;
            }
        }
    }
    return victim; /* -1 if no valid page found */
}