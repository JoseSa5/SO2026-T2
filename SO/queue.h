#ifndef QUEUE_H
#define QUEUE_H
#include <stdint.h>

#define MAX_CLIENTS 128

#include "pcb.h"
#include "virtmem_types.h"

 
typedef struct queue_elem_st queue_elem_t;

typedef struct queue_elem_st {
    pcb_t *pcb;
    struct queue_elem_st *next;
} queue_elem_t;

 
 
typedef struct queue_st  {
    queue_elem_t* head;
    queue_elem_t* tail;
    ssize_t size;
} queue_t;

uint32_t now_ms(void);
void reset_time(void);

 






pcb_t *new_pcb(int32_t pid, uint32_t sockfd, uint32_t time_ms);

 



int enqueue_pcb(queue_t* q, pcb_t* task);

 



pcb_t* dequeue_pcb(queue_t* q);

 




queue_elem_t *remove_queue_elem(queue_t* q, queue_elem_t* elem);

void check_blocked_queue(queue_t *blocked_queue, queue_t *command_queue, uint32_t current_time_ms);

void check_new_commands(queue_t *command_queue, queue_t *blocked_queue, queue_t *ready_queue,
                        int server_fd, uint32_t current_time_ms,
                        frame_table_t *frame_table, swap_hash_t *swap);

ssize_t receive_msg(int sockfd, void *msg, ssize_t msg_len);

int setup_server_socket(const char *socket_path);
void set_page_limit(int limit);

#endif  
