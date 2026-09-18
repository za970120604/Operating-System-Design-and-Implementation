#include "sched.h"
#include "exception.h"
#include "memory.h"
#include "timer.h"
#include "uart1.h"
#include "signal.h"
#include "mmu.h"
#include "string.h"

thread_t *curr_thread;
thread_t *RQ_head = NULL;
thread_t *RQ_tail = NULL;
thread_t threads[MAX_THREAD_COUNT + 1];
extern int SAFE_DEMO;

void push_rq(thread_t* th) {
    disable_interrupt();
    
    if (RQ_tail != NULL) {
        th->prev = RQ_tail;
        RQ_tail->next = th;
        RQ_tail = th;
    }
    else {
        RQ_head = RQ_tail = th;
    }
    
    th->next = NULL;
    
    enable_interrupt();
}

thread_t* pop_rq() {
    disable_interrupt();
    
    thread_t* target_thread = RQ_head;
    
    if (RQ_head != NULL) {
        if (RQ_head->next == NULL) {
            RQ_tail = NULL;
        }
        else {
            RQ_head->next->prev = NULL;
        }
        RQ_head = RQ_head->next;
    }
    
    if (target_thread != NULL) {
        target_thread->prev = NULL;
        target_thread->next = NULL;
    }
    
    enable_interrupt();
    return target_thread;
}

void init_thread_sched() {
    disable_interrupt();
    for (int i = 0; i <= MAX_THREAD_COUNT; i++) {
        threads[i].pid = i;
        threads[i].status = THREAD_FREE;
        threads[i].prev = NULL;
        threads[i].next = NULL;
    }
    
    thread_t* idlethread = thread_create(idle, 0x1000);
    curr_thread = idlethread;
    asm volatile("msr tpidr_el1, %0" ::"r" (&idlethread->context));
    enable_interrupt();
}

void idle() {
    while(1) {
        kill_zombies();   
        schedule();
    }
}

void schedule() {
    disable_interrupt();
    if (RQ_head == NULL) {
        enable_interrupt();
        return;
    }
    
    thread_t* off_thread = curr_thread;
    thread_t* target_thread;
    
    do {
        target_thread = pop_rq();
        if (target_thread == NULL) {
            enable_interrupt();
            return;
        }
        
        if (target_thread->status == THREAD_ZOMBIE) {
            push_rq(target_thread);
        }
    } while (target_thread->status == THREAD_ZOMBIE);
    
    curr_thread = target_thread;

    if (off_thread->status != THREAD_ZOMBIE) {
        push_rq(off_thread);
    }
    
    enable_interrupt();
    switch_to(get_current(), &curr_thread->context);
}

void kill_zombies() {
    disable_interrupt();
    // int flag = 0;
    thread_t* th_ptr = RQ_head;
    thread_t* next_ptr;
    while (th_ptr != NULL) {
        next_ptr = th_ptr->next;  
        
        if (th_ptr->status == THREAD_ZOMBIE) {

            if (!SAFE_DEMO) {
                uart_sendline("Idle thread find zombie ");
                uart_binary_to_hex_long(th_ptr->pid);
                uart_sendline("\r\n");
            }
            
            if (th_ptr == RQ_head) {
                RQ_head = th_ptr->next;
            }
            if (th_ptr == RQ_tail) {
                RQ_tail = th_ptr->prev;
            }
            
            if (th_ptr->prev != NULL) {
                th_ptr->prev->next = th_ptr->next;
            }
            if (th_ptr->next != NULL) {
                th_ptr->next->prev = th_ptr->prev;
            }
            
            mmu_free_page_tables(th_ptr->context.pgd, 0);
            mmu_del_all_vma(th_ptr);
            ////////////////////////////////////
            for(int i = 0; i < MAX_FD;i++)
            {
                if (th_ptr->file_descriptors_table[i])
                    vfs_close(th_ptr->file_descriptors_table[i]);
            }
            ////////////////////////////////////
            kfree(th_ptr->kernel_sp);
            kfree(PHYS_TO_VIRT(th_ptr->context.pgd));
            
            th_ptr->status = THREAD_FREE;
            th_ptr->prev = NULL;
            th_ptr->next = NULL;
        }
        
        th_ptr = next_ptr;
    }
    // if (flag) {
    //     debug_print_queue();
    // }
    enable_interrupt();
}

thread_t *thread_create(void *start, unsigned int filesize) {
    disable_interrupt();
    
    thread_t *new_thread = NULL;
    for (int i = 0; i <= MAX_THREAD_COUNT; i++) {
        if (threads[i].status == THREAD_FREE) {
            new_thread = &threads[i];
            break;
        }
    }
    
    if (new_thread == NULL) {
        enable_interrupt();
        return NULL;
    }

    //////////////////////////////////////
    // INIT_LIST_HEAD(&new_thread->vma_list);
    new_thread->vma_list = NULL;
    /////////////////////////////////////
    new_thread->status = THREAD_ACTIVE;
    new_thread->context.lr = (unsigned long long)start;
    new_thread->user_sp = kmalloc(USTACK_SIZE);
    new_thread->kernel_sp = kmalloc(KSTACK_SIZE);
    ///////////////////////////////////////////
    new_thread->signal_sp = kmalloc(FRAME_SIZE);
    ///////////////////////////////////////////
    new_thread->signal_is_checking = 0;
    new_thread->data = kmalloc(filesize);
    new_thread->datasize = filesize;
    new_thread->context.sp = (unsigned long long)new_thread->kernel_sp + KSTACK_SIZE;
    new_thread->context.fp = new_thread->context.sp;
    
    //////////////////////////////////
    strcpy(new_thread->curr_working_dir, "/");
    //////////////////////////////////

    new_thread->context.pgd = kmalloc(0x1000);
    utils_memset(new_thread->context.pgd, 0, 0x1000);
    
    for (int i = 0; i < SIGNAL_MAX; i++) {
        new_thread->signal_handler[i] = signal_default_handler;
        new_thread->sigcount[i] = 0;
    }
    
    push_rq(new_thread);
    enable_interrupt();
    return new_thread;
}

int thread_exec(char *data, unsigned int filesize) {
    thread_t *t = thread_create(data, filesize);

    mmu_add_vma(t, USER_KERNEL_BASE, t->datasize, (size_t)VIRT_TO_PHYS(t->data), 0b111, 1);
    mmu_add_vma(t, USER_STACK_BASE - USTACK_SIZE, USTACK_SIZE, (size_t)VIRT_TO_PHYS(t->user_sp), 0b111, 1);
    mmu_add_vma(t, PERIPHERAL_START, PERIPHERAL_END - PERIPHERAL_START, PERIPHERAL_START, 0b011, 0);
    mmu_add_vma(t, USER_SIGNAL_WRAPPER_VA, 0x2000, (size_t)VIRT_TO_PHYS(signal_handler_wrapper), 0b101, 0);

    /////////////////////////////////////////////////////////////
    mmu_add_vma(t, USER_SIGNAL_STKSTART_VA, FRAME_SIZE, (size_t)VIRT_TO_PHYS(t->signal_sp), 0b111, 1);
    /////////////////////////////////////////////////////////////

    // 設定thread context的PGD, sp, fp, lr
    t->context.pgd = VIRT_TO_PHYS(t->context.pgd);
    t->context.sp = USER_STACK_BASE;
    t->context.fp = USER_STACK_BASE;
    t->context.lr = USER_KERNEL_BASE;

    //copy file into data
    for (int i = 0; i < filesize; i++) {
        t->data[i] = data[i];
    }

    //disable echo when going to userspace
    curr_thread = t;

    //////////////////////////////////////
    vfs_open("/dev/uart", 0, &curr_thread->file_descriptors_table[0]); // stdin
    vfs_open("/dev/uart", 0, &curr_thread->file_descriptors_table[1]); // stdout
    vfs_open("/dev/uart", 0, &curr_thread->file_descriptors_table[2]); // stderr
    //////////////////////////////////////

    add_timer(schedule_timer, 1, "", 0);
    
    // set tpidr to thread context_t
    // spsr_el1 
    // 0~3 bit 0b0000 : el0t , jump to el0 and use el0 stack
    // 6~9 bit 0b0000 : turn on every interrupt

    // elr_el1 : 設定program的起始位置給elr_el1
    
    // sp_el0 : user mode stack pointer, stack pointer set to top of program
    
    // sp : set kernel stack pointer, current is in el1 , so = sp_el1
    // eret to exception level 0
    asm("msr tpidr_el1, %0\n\t" // 將 %0 (t->context)的值寫入 TPIDR_EL1
        "msr elr_el1, %1\n\t"   // 將 %1 (t->data)的值寫入 ELR_EL1
        "msr spsr_el1, xzr\n\t" // 將 xzr (清零)的值寫入 SPSR_EL1, enable interrupt in EL0
        "msr sp_el0, %2\n\t"    // 將 %2 (t->user_sp + USTACK_SIZE)的值寫入 SP_EL0
        "mov sp, %3\n\t"        // 將 %3 (t->kernel_sp + KSTACK_SIZE)的值移動到 SP
        "dsb ish\n\t"           // 確保寫入完成
        "msr ttbr0_el1, %4\n\t" // switch translation based address
        "tlbi vmalle1is\n\t" // invalidate 所有 TLB entries
        "dsb ish\n\t"        // 確保 TLB invalidatation的完成
        "isb\n\t"            // clear pipeline
        "eret\n\t" ::"r"(&t->context),"r"(t->context.lr), "r"(t->context.sp), "r"(t->kernel_sp + KSTACK_SIZE), "r"(t->context.pgd));

    return 0;
}

void thread_exit() {
    disable_interrupt();
    curr_thread->status = THREAD_ZOMBIE;
    enable_interrupt();
    schedule();
}


void schedule_timer(char* notuse){
    unsigned long long cntfrq_el0;
    __asm__ __volatile__("mrs %0, cntfrq_el0\n\t": "=r"(cntfrq_el0)); //tick frequency
    add_timer(schedule_timer, cntfrq_el0 >> 5, "", 1);
}
