#include "syscall.h"
#include "cpio.h"
#include "sched.h"
#include "stddef.h"
#include "uart1.h"
#include "exception.h"
#include "memory.h"
#include "mbox.h"
#include "signal.h"
#include "mmu.h"
#include "string.h"

extern int SAFE_DEMO;

int getpid(trapframe_t* tpf) {
    tpf->x0 = curr_thread->pid;
    return curr_thread->pid;
}

unsigned long uartread(trapframe_t *tpf, char buf[], unsigned long size) {
    unsigned long count = 0;
    for (; count < size; count++) {
        if (!SAFE_DEMO) {
            buf[count] = uart_async_getc();
        }
        else {
            buf[count] = uart_recv();
        }
    }
    tpf->x0 = count;
    return count;
}

unsigned long uartwrite(trapframe_t *tpf, const char buf[], unsigned long size) {
    unsigned long count = 0;
    for (; count < size; count++) {
        if (!SAFE_DEMO) {
            uart_async_putc(buf[count]);
        }
        else {
            uart_send(buf[count]);
        }
    }
    tpf->x0 = count;
    return count;
}

int exec(trapframe_t *tpf,const char *name, char *const argv[]) {
    mmu_del_all_vma(curr_thread);
    curr_thread->vma_list = NULL;
    
    //////////////////////////////////////////////////////
    // char* new_data;
    // cpio_find_program((char*)name, &(curr_thread->datasize), &new_data);

    char abs_path[MAX_PATH_NAME];
    strcpy(abs_path, name);
    get_absolute_path(abs_path, curr_thread->curr_working_dir);

    struct vnode *target_file;
    vfs_lookup(abs_path,&target_file);
    curr_thread->datasize = target_file->f_ops->getsize(target_file);
    curr_thread->data = kmalloc(curr_thread->datasize);
    //////////////////////////////////////////////////////
    
    curr_thread->data = kmalloc(curr_thread->datasize);
    curr_thread->user_sp = kmalloc(USTACK_SIZE);
    curr_thread->signal_sp = kmalloc(FRAME_SIZE);

    asm("dsb ish\n\t");
    
    mmu_free_page_tables(curr_thread->context.pgd, 0);
    utils_memset(PHYS_TO_VIRT(curr_thread->context.pgd), 0, 0x1000);
    
    asm("tlbi vmalle1is\n\t" // invalidate all TLB entries
        "dsb ish\n\t"        // ensure completion of TLB invalidatation
        "isb\n\t");          // clear pipeline

    mmu_add_vma(curr_thread, USER_KERNEL_BASE, curr_thread->datasize, (size_t)VIRT_TO_PHYS(curr_thread->data), 0b111, 1);
    mmu_add_vma(curr_thread, USER_STACK_BASE - USTACK_SIZE, USTACK_SIZE, (size_t)VIRT_TO_PHYS(curr_thread->user_sp), 0b111, 1);
    mmu_add_vma(curr_thread, PERIPHERAL_START, PERIPHERAL_END - PERIPHERAL_START, PERIPHERAL_START, 0b011, 0);
    mmu_add_vma(curr_thread, USER_SIGNAL_WRAPPER_VA, 0x2000, (size_t)VIRT_TO_PHYS(signal_handler_wrapper), 0b101, 0);
    mmu_add_vma(curr_thread, USER_SIGNAL_STKSTART_VA, FRAME_SIZE, (size_t)VIRT_TO_PHYS(curr_thread->signal_sp), 0b111, 1);


    //////////////////////////////////////////////////////
    // utils_memcpy(curr_thread->data, new_data, curr_thread->datasize);
    struct file *f;
    vfs_open(abs_path, 0, &f);
    vfs_read(f, curr_thread->data, curr_thread->datasize);
    vfs_close(f);
    //////////////////////////////////////////////////////
    
    // clear signal handler
    for (int i = 0; i <= SIGNAL_MAX; i++){
        curr_thread->signal_handler[i] = signal_default_handler;
    }

    // set program (exception return address)
    tpf->elr_el1 = USER_KERNEL_BASE;
    
    // set user stack pointer (el0)
    tpf->sp_el0 = USER_STACK_BASE;
    
    // set return value
    tpf->x0 = 0;
    return 0;
}

int fork(trapframe_t *tpf) {
    disable_interrupt();
    thread_t *newt = thread_create(curr_thread->data,curr_thread->datasize);

    // copy signal handler
    for (int i = 0; i <= SIGNAL_MAX;i++) {
        newt->signal_handler[i] = curr_thread->signal_handler[i];
    }

    ////////////////////////////////////////////////////
    for (int i = 0; i <= MAX_FD; i++) {
        if (curr_thread->file_descriptors_table[i]) {
            newt->file_descriptors_table[i] = kmalloc(sizeof(struct file));
            *newt->file_descriptors_table[i] = *curr_thread->file_descriptors_table[i];
        }
    }
    ////////////////////////////////////////////////////

    vm_area_struct_t *current = curr_thread->vma_list;
    while (current != NULL) {
        // ignore device and signal wrapper
        if (current->virt_addr == USER_SIGNAL_WRAPPER_VA || 
            current->virt_addr == PERIPHERAL_START || 
            current->virt_addr == USER_SIGNAL_STKSTART_VA) {
            current = current->next;
            continue;
        }
        char *new_alloc = kmalloc(current->area_size);
        mmu_add_vma(newt, current->virt_addr, current->area_size, (size_t)VIRT_TO_PHYS(new_alloc), current->rwx, 1);
        utils_memcpy(new_alloc, (void*)PHYS_TO_VIRT(current->phys_addr), current->area_size);

        if (current->virt_addr == USER_KERNEL_BASE) {
            kfree(newt->data);
            newt->data = new_alloc;
        }
        else if (current->virt_addr == USER_STACK_BASE - USTACK_SIZE) {
            kfree(newt->user_sp);
            newt->user_sp = new_alloc;
        }
        
        current = current->next;
    }

    mmu_add_vma(newt, PERIPHERAL_START, PERIPHERAL_END - PERIPHERAL_START, PERIPHERAL_START, 0b011, 0);
    mmu_add_vma(newt, USER_SIGNAL_WRAPPER_VA, 0x2000, (size_t)VIRT_TO_PHYS(signal_handler_wrapper), 0b101, 0);
    mmu_add_vma(newt, USER_SIGNAL_STKSTART_VA, FRAME_SIZE, (size_t)VIRT_TO_PHYS(newt->signal_sp), 0b111, 1);

    // store parent pid
    int parent_pid = curr_thread->pid;

    // copy kernel stack, user stack is copied by copying vma at the same time.
    for (int i = 0; i < KSTACK_SIZE; i++){
        newt->kernel_sp[i] = curr_thread->kernel_sp[i];
    }
    
    store_context(get_current());

    if (parent_pid != curr_thread->pid) { // split parent and child's execution flow
        goto child;
    }

    // set child thread's context
    void *temp_pgd = newt->context.pgd;
    newt->context = curr_thread->context;
    newt->context.pgd = VIRT_TO_PHYS(temp_pgd);
    newt->context.fp += newt->kernel_sp - curr_thread->kernel_sp; // move fp
    newt->context.sp += newt->kernel_sp - curr_thread->kernel_sp; // move kernel sp

    enable_interrupt();
    
    // return child process pid
    tpf->x0 = newt->pid;
    return newt->pid;

child:
    // child return 0
    tpf->x0 = 0;
    return 0;
}

void exit(trapframe_t *tpf, int status) {
    disable_interrupt();
    curr_thread->status = THREAD_ZOMBIE;
    enable_interrupt();
}

int syscall_mbox_call(trapframe_t *tpf, unsigned char ch, unsigned int *mbox_user) {
    disable_interrupt();

    unsigned int size_of_mbox = mbox_user[0];
    utils_memcpy((char *)pt, (char *)mbox_user, size_of_mbox);
    mailbox_call(pt);
    utils_memcpy((char *)mbox_user, (char *)pt, size_of_mbox);

    tpf->x0 = 8;
    enable_interrupt();
    return 0;
}

void kill(trapframe_t *tpf,int pid) {
    disable_interrupt();
    if (pid >= MAX_THREAD_COUNT || pid < 0  || threads[pid].status == THREAD_FREE) {
        enable_interrupt();
        return;
    }

    if (!SAFE_DEMO) {
        uart_sendline("Kill thread pid ");
        uart_binary_to_hex_long(pid);
        uart_sendline("\r\n");
    }

    threads[pid].status = THREAD_ZOMBIE;
    enable_interrupt();
    schedule();
}

void signal_register(int signal, void (*handler)()) {
    if (signal > SIGNAL_MAX || signal < 0)return;

    if (!SAFE_DEMO) {  
        uart_sendline("[DEBUG] register signal in thread id ");
        uart_binary_to_hex_long(curr_thread->pid);
        uart_sendline(", signal handler address at ");
        uart_binary_to_hex_long((unsigned long)(handler));
        uart_sendline("\r\n");
    }

    curr_thread->signal_handler[signal] = handler;
}

void signal_kill(int pid, int signal) {
    if (pid > MAX_THREAD_COUNT || pid < 0 || threads[pid].status == THREAD_FREE) {
        return;
    }

    if (!SAFE_DEMO) {  
        uart_sendline("[DEBUG] signal kill is called in thread id ");
        uart_binary_to_hex_long(pid);
        uart_sendline("\r\n");
    }

    disable_interrupt();
    threads[pid].sigcount[signal]++;
    enable_interrupt();
}

// only need to implement the anonymous page mapping in this Lab.
void *mmap(trapframe_t *tpf, void *addr, size_t len, int prot, int flags, int fd, int file_offset) {
    // Ignore flags as we have demand pages

    uart_sendline("Syscall mmap, addr: ");
    uart_binary_to_hex_long((unsigned long)addr);
    uart_sendline(", len: ");
    uart_binary_to_hex_long((unsigned long)len);
    uart_sendline(", prot: ");
    uart_binary_to_hex_long((unsigned long)prot);
    uart_sendline(", flags: ");
    uart_binary_to_hex_long((unsigned long)flags);
    uart_sendline("\r\n");
    
    // round up length and addr to 4KB aligned
    len = len % 0x1000 ? len + (0x1000 - len % 0x1000) : len;
    addr = (unsigned long)addr % 0x1000 ? addr + (0x1000 - (unsigned long)addr % 0x1000) : addr;

    vm_area_struct_t *current = curr_thread->vma_list;
    vm_area_struct_t *the_area_ptr = NULL;
    
    while (current != NULL) {
        // check if overlapped with current existing vma
        if (!(current->virt_addr >= (unsigned long)(addr + len) || 
              current->virt_addr + current->area_size <= (unsigned long)addr)) {
            the_area_ptr = current;
            break;
        }
        current = current->next;
    }
    
    // if overlapped, take as a hint to decide new region's start address
    if (the_area_ptr) {
        tpf->x0 = (unsigned long) mmap(tpf, (void *)(the_area_ptr->virt_addr + the_area_ptr->area_size), len, prot, flags, fd, file_offset);
        return (void *)tpf->x0;
    }

    // set correct rwx permission
    size_t rwx = 0;
    if (prot & 1) rwx |= 0b001; // PROT_READ  -> readable
    if (prot & 2) rwx |= 0b010; // PROT_WRITE -> writable
    if (prot & 4) rwx |= 0b100; // PROT_EXEC  -> executable
    
    // create vma and actually allocate physical memory using demand paging
    void* mmap_ptr = kmalloc(len);
    utils_memset(mmap_ptr, 0, len);
    unsigned long mmap_addr = (unsigned long)mmap_ptr;
    mmu_add_vma(curr_thread, (unsigned long)addr, len, VIRT_TO_PHYS(mmap_addr), rwx, 1);
    tpf->x0 = (unsigned long)addr;

    // return new region's address
    return (void*)tpf->x0;
}

void sigreturn(trapframe_t *tpf) {
    //unsigned long signal_ustack = tpf->sp_el0 % USTACK_SIZE == 0 ? tpf->sp_el0 - USTACK_SIZE : tpf->sp_el0 & (~(USTACK_SIZE - 1));
    //kfree((char*)signal_ustack);
    load_context(&curr_thread->signal_saved_context);
}

int open(trapframe_t *tpf, const char *pathname, int flags) {
    uart_sendline("[syscall] call \"open\" syscall with pathname ");
    uart_sendline((char*)pathname);
    uart_sendline(" and flags ");
    uart_binary_to_hex_long(flags);
    uart_sendline("\r\n");

    char abs_path[MAX_PATH_NAME];
    strcpy(abs_path, pathname);
    get_absolute_path(abs_path, curr_thread->curr_working_dir);

    for (int i = 0; i < MAX_FD; i++) {
        // find a usable fd
        if(!curr_thread->file_descriptors_table[i]) {
            if (vfs_open(abs_path, flags, &curr_thread->file_descriptors_table[i]) != 0 ){
                break;
            }
            tpf->x0 = i;
            return i;
        }
    }

    tpf->x0 = -1;
    return -1;
}

int close(trapframe_t *tpf, int fd) {
    uart_sendline("[syscall] call \"close\" syscall with fd ");
    uart_binary_to_hex_long(fd);
    uart_sendline("\r\n");

    if (curr_thread->file_descriptors_table[fd]) {
        // close using vfs_close and clear the fd slot
        vfs_close(curr_thread->file_descriptors_table[fd]);
        curr_thread->file_descriptors_table[fd] = 0;
        tpf->x0 = 0;
        return 0;
    }

    tpf->x0 = -1;
    return -1;
}

long write(trapframe_t *tpf, int fd, const void *buf, unsigned long count) {
    // uart_sendline("curr_thread: ");
    // uart_binary_to_hex_long(curr_thread->pid);
    // uart_sendline(" call \"write\" syscall with fd ");
    // uart_binary_to_hex_long(fd);
    // uart_sendline(" and with len ");
    // uart_binary_to_hex_long(count);
    // uart_sendline("\r\n");

    if (curr_thread->file_descriptors_table[fd]) {
        tpf->x0 = vfs_write(curr_thread->file_descriptors_table[fd], buf, count);
        return tpf->x0;
    }

    tpf->x0 = -1;
    return tpf->x0;
}

long read(trapframe_t *tpf, int fd, void *buf, unsigned long count) {
    // uart_sendline("curr_thread: ");
    // uart_binary_to_hex_long(curr_thread->pid);
    // uart_sendline(" call \"read\" syscall with fd ");
    // uart_binary_to_hex_long(fd);
    // uart_sendline(" and with len ");
    // uart_binary_to_hex_long(count);
    // uart_sendline("\r\n");

    if (curr_thread->file_descriptors_table[fd]) {
        tpf->x0 = vfs_read(curr_thread->file_descriptors_table[fd], buf, count);
        return tpf->x0;
    }

    tpf->x0 = -1;
    return tpf->x0;
}

int mkdir(trapframe_t *tpf, const char *pathname, unsigned mode) {
    uart_sendline("[syscall] call \"mkdir\" syscall with pathname ");
    uart_sendline((char*)pathname);
    uart_sendline("\r\n");

    char abs_path[MAX_PATH_NAME];
    strcpy(abs_path, pathname);
    get_absolute_path(abs_path, curr_thread->curr_working_dir);

    tpf->x0 = vfs_mkdir(abs_path);
    return tpf->x0;
}

int mount(trapframe_t *tpf, const char *src, const char *target, const char *filesystem, unsigned long flags, const void *data) {
    uart_sendline("[syscall] call \"mount\" syscall with target ");
    uart_sendline((char*)target);
    uart_sendline(", fs ");
    uart_sendline((char*)filesystem);
    uart_sendline("\r\n");

    char abs_path[MAX_PATH_NAME];
    strcpy(abs_path, target);
    get_absolute_path(abs_path, curr_thread->curr_working_dir);

    tpf->x0 = vfs_mount(abs_path,filesystem);
    return tpf->x0;
}

int chdir(trapframe_t *tpf, const char *path) {
    uart_sendline("[syscall] call \"chdir\" syscall with path ");
    uart_sendline((char*)path);
    uart_sendline("\r\n");

    char abs_path[MAX_PATH_NAME];
    strcpy(abs_path, path);
    get_absolute_path(abs_path, curr_thread->curr_working_dir);

    strcpy(curr_thread->curr_working_dir, abs_path);
    return 0;
}
