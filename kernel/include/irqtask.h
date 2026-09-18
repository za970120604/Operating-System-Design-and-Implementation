#ifndef _IRQTASK_H_
#define _IRQTASK_H_

#define UART_IRQ_PRIORITY  1
#define TIMER_IRQ_PRIORITY 0

typedef struct irqtask {
    unsigned long long priority;
    void *task_function;
    struct irqtask* next;
    struct irqtask* prev;
} irqtask_t;


void irqtask_add(void *task_function, unsigned long long priority);
void irqtask_run_preemptive();
void irqtask_init_list();

#endif
