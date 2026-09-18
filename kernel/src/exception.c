#include "bcm2837/rpi_uart1.h"
#include "uart1.h"
#include "exception.h"
#include "timer.h"
#include "irqtask.h"
#include "syscall.h"
#include "sched.h"
#include "signal.h"
#include "mmu.h"
static unsigned long long nested_interrupt_cnt = 0;
extern int SAFE_DEMO;

void sync_64_router(trapframe_t* tpf) {
    unsigned long long esr_el1;
    __asm__ __volatile__("mrs %0, esr_el1\n\t": "=r"(esr_el1));
    // esr_el1: Holds syndrome information for an exception taken to EL1.
    esr_el1_t *esr = (esr_el1_t *)&esr_el1;
    if (esr->ec == MEMFAIL_DATA_ABORT_LOWER || esr->ec == MEMFAIL_INST_ABORT_LOWER) {
        mmu_memfail_abort_handle(esr);
        return;
    }

    el1_interrupt_enable();
    unsigned long long syscall_number = tpf->x8;                                                            
    switch (syscall_number) {
        case 0: 
            getpid(tpf);
            // uart_sendline("syscall 'pid' is called\r\n");
            break;
        case 1: 
            if (SAFE_DEMO) {
                disable_interrupt();
            }

            uartread(tpf,(char *) tpf->x0, tpf->x1);  
            // uart_sendline("syscall 'uartread' is called\r\n");

            if (SAFE_DEMO) {
                enable_interrupt();
            }
            break;
        case 2:

            if (SAFE_DEMO) {
                disable_interrupt();
            }

            uartwrite(tpf,(char *) tpf->x0, tpf->x1);

            if (SAFE_DEMO) {
                enable_interrupt();
            }
            // uart_sendline("syscall 'uartwrite' is called\r\n");
            break;
        case 3:
            exec(tpf,(char *) tpf->x0, (char **)tpf->x1);  
            // uart_sendline("syscall 'exec' is called\r\n");
            break;
        case 4:
            fork(tpf); 
            // uart_sendline("syscall 'fork' is called\r\n");
            break;
        case 5:
            exit(tpf,tpf->x0);  
            // uart_sendline("syscall 'exit' is called\r\n");
            break;
        case 6:
            syscall_mbox_call(tpf,(unsigned char)tpf->x0, (unsigned int *)tpf->x1); 
            // uart_sendline("syscall 'mbox_call' is called\r\n");
            break;
        case 7:
            kill(tpf, (int)tpf->x0);
            // uart_sendline("syscall 'kill' is called\r\n");
            break;
        case 8:
            signal_register(tpf->x0, (void (*)())tpf->x1); 
            // uart_sendline("syscall 'signal_register' is called\r\n");
            break;
        case 9:
            signal_kill(tpf->x0, tpf->x1); 
            // uart_sendline("syscall 'signal_kill' is called\r\n");
            break;
        case 10:
            mmap(tpf,(void *)tpf->x0,tpf->x1,tpf->x2,tpf->x3,tpf->x4,tpf->x5); 
            // uart_sendline("syscall 'mmap' is called\r\n");
            break;
        case 50:
            // uart_sendline("syscall 'sigreturn' is called\r\n");
            sigreturn(tpf); 
            break;
        case 11:
            open(tpf, (char*)tpf->x0, tpf->x1);    
            break;                                                 
        case 12:
            close(tpf, tpf->x0);    
            break;                                    
        case 13:
            write(tpf, tpf->x0, (char *)tpf->x1, tpf->x2);
            break;                                         
        case 14: 
            read(tpf, tpf->x0, (char *)tpf->x1, tpf->x2);
            break;                     
        case 15:
            mkdir(tpf, (char *)tpf->x0, tpf->x1);  
            break;
        case 16:
            disable_interrupt();
            mount(tpf, (char *)tpf->x0, (char *)tpf->x1, (char *)tpf->x2, tpf->x3, (void*)tpf->x4);     
            enable_interrupt();                                            
            break;
        case 17:
            chdir(tpf, (char *)tpf->x0); 
            break;               
        default:
            // uart_sendline("Unknown syscall number\r\n");
            break;
    }
    el1_interrupt_disable();
}

void irq_router(trapframe_t* tpf) {
    if (*IRQ_PENDING_1 & IRQ_PENDING_1_AUX_INT && *CORE0_INTERRUPT_SOURCE & INTERRUPT_SOURCE_GPU) {
        if (*AUX_MU_IIR_REG & (1 << 1)) // can write
        {
            *AUX_MU_IER_REG &= ~(2);  // disable write interrupt
            irqtask_add(uart_w_irq_handler, UART_IRQ_PRIORITY);
            irqtask_run_preemptive();
        }
        else if (*AUX_MU_IIR_REG & (0b10 << 1)) // can read
        {
            *AUX_MU_IER_REG &= ~(1);  // disable read interrupt
            irqtask_add(uart_r_irq_handler, UART_IRQ_PRIORITY);
            irqtask_run_preemptive();
        }
    } 
    else if(*CORE0_INTERRUPT_SOURCE & INTERRUPT_SOURCE_CNTPNSIRQ) {
        core_timer_disable();
        irqtask_add(core_timer_handler, TIMER_IRQ_PRIORITY);
        irqtask_run_preemptive();
        core_timer_enable();
        el1_interrupt_disable();
        schedule();
    }
    el1_interrupt_disable();
}

void irq_router2(trapframe_t* tpf) {
    if (SAFE_DEMO) {
        if(*CORE0_INTERRUPT_SOURCE & INTERRUPT_SOURCE_CNTPNSIRQ) {
            core_timer_disable();
            irqtask_add(core_timer_handler, TIMER_IRQ_PRIORITY);
            irqtask_run_preemptive();
            core_timer_enable();
            schedule();
        }
    }
    else {
        irq_router(tpf);
    }
}

void invalid_exception_router(unsigned long long x0){
    // uart_sendline("Invalid IRQ found, ");
    // uart_binary_to_hex_long(x0);
    // uart_sendline("\r\n");
}

void enable_interrupt() {
    if (nested_interrupt_cnt > 0) {
        nested_interrupt_cnt--;
    }

    if (nested_interrupt_cnt == 0) {
        asm volatile("msr DAIFClr, 0xf;");
    }
}

void disable_interrupt() {
    if (nested_interrupt_cnt == 0) {
        asm volatile("msr DAIFSet, 0xf;");
    }   
    
    nested_interrupt_cnt++;
}