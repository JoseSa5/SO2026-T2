 
 
 

#ifndef PCB_H
#define PCB_H
#include <stdint.h>

#include "msg.h"
#include "virtmem_types.h"

typedef enum  {
    TASK_COMMAND = 0,    
    TASK_BLOCKED,        
    TASK_RUNNING,        
    TASK_STOPPED,        
    TASK_TERMINATED,     
} task_status_en;

 
typedef struct pcb_st{
    int32_t pid;                    
    task_status_en status;          
    uint32_t time_ms;               
    uint32_t ellapsed_time_ms;      
    uint32_t slice_start_ms;        
    uint32_t sockfd;                
    uint32_t last_update_time_ms;   

    page_info_t requested_pages;    
    page_table_t page_table;        
} pcb_t;

#endif  
