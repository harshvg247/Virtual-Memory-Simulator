#ifndef SCHEDULER_H
#define SCHEDULER_H

/* scheduler.h
 * FCFS Scheduler interface.
 *
 * CLI usage:
 *   scheduler <mq_ready_key> <mq_sched_key> <num_procs>
 *
 * Where:
 *   mq_ready_key : key for ready queue (MQ1)
 *   mq_sched_key : key for scheduler<->MMU communication (MQ2)
 *   num_procs    : number of processes to schedule
 */

int scheduler_run(int mq_ready_key, int mq_sched_key, int num_procs);

#endif /* SCHEDULER_H */
