#ifndef COMMON_H
#define COMMON_H

 
 

#define TICKS_MS 10

#include <stdint.h>
#include <sys/types.h>

#define SOCKET_PATH "/tmp/scheduler.sock"

#define MAX_PAGES 60

 
static const char PROCESS_REQUEST_STRINGS[][10] = {
    "RUN",
    "BLOCK",
    "ACK",
    "DONE"
};

 
typedef enum  {
    PROCESS_REQUEST_RUN = 0,
    PROCESS_REQUEST_BLOCK,
    PROCESS_REQUEST_ACK,
    PROCESS_REQUEST_DONE,
} process_request_t;

 
 
typedef struct {
    uint32_t count;             
    int32_t ids[MAX_PAGES];    
} page_info_t;

 
 
typedef struct {
    pid_t pid;                       
    process_request_t request;       
    uint32_t time_ms;                
    page_info_t pages;               
} msg_t;


#endif  
