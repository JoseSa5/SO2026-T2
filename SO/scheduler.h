#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "queue.h"

 
#define TIME_SLICE_MS 500

 
 
typedef enum {
    SCHED_FIFO = 0,
} sched_algo_en;

int  set_sched_algo(const char *name);    
const char *get_sched_algo_str(void);

 








int scheduler(uint32_t current_time_ms, queue_t *rq, queue_t *cq, pcb_t **cpu_task);

#endif  
