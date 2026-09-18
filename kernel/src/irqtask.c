#include "irqtask.h"
#include "exception.h"
#include "memory.h"
#include "uart1.h"

irqtask_t* task_head = NULL;
irqtask_t* task_tail = NULL;
int curr_task_priority = 9999;

void irqtask_init_list() {
    task_head = (irqtask_t*)kmalloc(sizeof(irqtask_t));
    task_tail = (irqtask_t*)kmalloc(sizeof(irqtask_t));
    
    task_head->priority = -1;
    task_head->next = task_tail;
    task_head->prev = NULL;

    task_tail->priority = 9999;
    task_tail->next = NULL;
    task_tail->prev = task_head;
}

int is_task_queue_empty() {
    return task_head->next == task_tail;
}

void irqtask_add(void *task_function, unsigned long long priority) {
    irqtask_t *new_task = kmalloc(sizeof(irqtask_t));
    new_task->priority = priority;
    new_task->task_function = task_function;
    
    disable_interrupt();
    irqtask_t *current = task_head;
    
    while (current->next != NULL) {
        irqtask_t *next = (irqtask_t*)current->next;
        if (next != task_tail && priority < next->priority) {
            break;
        }
        
        if (next == task_tail) {
            break;
        }
            
        current = next;
    }
    
    irqtask_t *next = (irqtask_t*)current->next;
    
    new_task->next = next;
    new_task->prev = current;
    
    current->next = new_task;
    if (next != NULL) {
        next->prev = new_task;
    }
    
    enable_interrupt();
}

void irqtask_run_preemptive() {
    while (1) {
        disable_interrupt();
        if (is_task_queue_empty()) {
            enable_interrupt();
            break;
        }
        
        irqtask_t *the_task = (irqtask_t*)task_head->next;
        
        if (curr_task_priority <= the_task->priority) {
            enable_interrupt();
            break;
        }
        
        task_head->next = the_task->next;
        if (the_task->next != NULL) {
            the_task->next->prev = task_head;
        }
        
        int prev_task_priority = curr_task_priority;
        curr_task_priority = the_task->priority;
        
        enable_interrupt();
        ((void (*)())the_task->task_function)();
        disable_interrupt();
        
        kfree(the_task);
        curr_task_priority = prev_task_priority;
        
        enable_interrupt();
    }
}