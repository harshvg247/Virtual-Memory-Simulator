#ifndef PROCESS_H
#define PROCESS_H

/* process.h
 * Process-side interface: generate references and talk to MMU.
 *
 * CLI usage (called by master via fork/exec):
 *   process <pid> <mq_ready_key> <mq_proc_key> <ref_len> <ref_0> <ref_1> ... <ref_n>
 *
 * Arguments:
 *   pid          : unique process ID (0..k-1)
 *   mq_ready_key : MQ1 (ready queue key)
 *   mq_proc_key  : MQ3 (proc<->MMU key)
 *   ref_len      : number of page references
 *   ref_i        : each reference (page number, may include illegal values)
 */

int process_run(int mq_ready_key, int mq_proc_key,
                int ref_len, int *ref_str, int p_ind);

#endif /* PROCESS_H */
