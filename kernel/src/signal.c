#include "signal.h"
#include "sched.h"
#include "syscall.h"
#include "memory.h"
#include "mmu.h"
#include "uart1.h"

extern int SAFE_DEMO;

void check_signal(trapframe_t *tpf) {
    disable_interrupt();
    if(curr_thread->signal_is_checking) {
        enable_interrupt();
        return;
    }
    //prevent nested running signal handler
    curr_thread->signal_is_checking = 1;
    enable_interrupt();
    for (int i = 0; i <= SIGNAL_MAX; i++) {
        store_context(&curr_thread->signal_saved_context);
        if(curr_thread->sigcount[i]>0)
        {
            disable_interrupt();
            curr_thread->sigcount[i]--;
            enable_interrupt();
            run_signal(tpf,i);
        }
    }
    disable_interrupt();
    curr_thread->signal_is_checking = 0;
    enable_interrupt();
}

void run_signal(trapframe_t* tpf,int signal) {
    curr_thread->curr_signal_handler = curr_thread->signal_handler[signal];

    if (!SAFE_DEMO) {
        uart_sendline("[DEBUG] run signal in thread id ");
        uart_binary_to_hex_long(curr_thread->pid);
        uart_sendline(", signal handler address at ");
        uart_binary_to_hex_long((unsigned long)(curr_thread->curr_signal_handler));
        uart_sendline("\r\n");
    }

    //run default handler in kernel
    if (curr_thread->curr_signal_handler == signal_default_handler)
    {
        signal_default_handler();
        return;
    }

    asm("msr elr_el1, %0\n\t"
        "msr sp_el0, %1\n\t"
        "msr spsr_el1, %2\n\t"
	    "mov x0, %3\n\t"
        "eret\n\t"
        :: "r"(USER_SIGNAL_WRAPPER_VA + ((size_t)signal_handler_wrapper % 0x1000)),
           "r"(USER_SIGNAL_STKSTART_VA + FRAME_SIZE),
           "r"(tpf->spsr_el1),
           "r"(curr_thread->curr_signal_handler));
}

__attribute__((aligned(0x1000)))
void signal_handler_wrapper() {
    //elr_el1 set to function -> call function by x0
    //system call sigreturn
    asm("blr x0\n\t"
        "mov x8,50\n\t"
        "svc 0\n\t");
}

void debug_print_queue() {
    uart_sendline("---- Queue Status ----");
    thread_t* th = RQ_head;
    int count = 0;
    
    while (th != NULL && count < 10) {  // 限制最多輸出10個以防無限循環
        uart_sendline("Thread ID: ");
        uart_binary_to_hex_long(th->pid);
        uart_sendline(", Status: ");
        uart_binary_to_hex_long(th->status);
        uart_sendline("\r\n");
        
        th = th->next;
        count++;
    }
    
    uart_sendline("---- End of Queue ----\r\n");
}

void signal_default_handler() {
    disable_interrupt();

    if (!SAFE_DEMO) {
        uart_sendline("Signal default handler killing thread ");
        uart_binary_to_hex_long(curr_thread->pid);
        uart_sendline("\r\n");
    }
    
    curr_thread->status = THREAD_ZOMBIE;
    curr_thread->prev = NULL;
    curr_thread->next = NULL;
    
    push_rq(curr_thread);
    enable_interrupt();
    schedule();
}