#ifndef MASTER_H
#define MASTER_H

/* master.h
 * Entry point for the simulation.
 *
 * Usage:
 *   master <k> <m> <n> <ref_len>
 *
 * Where:
 *   k       : number of processes
 *   m       : max virtual pages per process
 *   n       : number of physical frames
 *   ref_len : length of reference string per process
 */

int master_run(int k, int m, int n, int ref_len);

#endif /* MASTER_H */
