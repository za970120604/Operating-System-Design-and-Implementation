#include "uart1.h"
#include "shell.h"
#include "dtb.h"
#include "memory.h"
#include "exception.h"
#include "irqtask.h"
#include "timer.h"
#include "sched.h"
extern void* dtb_base;

int SAFE_DEMO = 1; // change to 0 to use async uart to demo, but will prone to trigger error because of rapid print behavior.

int IF_QEMU = 0; // be sure to comment out the code in boot.S

void main(char* arg){
    if (IF_QEMU) {
        if (dtb_base == (void*)0) {
            void* dtb_phys;
            asm volatile("mov %0, x20" :  "=r"(dtb_phys));
            dtb_base = (void*)(PHYS_TO_VIRT((unsigned long long)dtb_phys));
            uart_sendline("DTB physical address: ");
            uart_binary_to_hex_long((unsigned long)dtb_phys);
            uart_sendline("\r\n");
            uart_sendline("DTB virtual address: ");
            uart_binary_to_hex_long((unsigned long)dtb_base);
            uart_sendline("\r\n");
        }
    }

    uart_init();
    // uart_sendline("shell1\r\n");
    fdt_traverse(initramfs_callback, "chosen", "linux,initrd-start"); // dtb, cpio address are both virtual
    fdt_traverse(initramfs_callback, "chosen", "linux,initrd-end"); // dtb, cpio address are both virtual
    // uart_sendline("shell2\r\n");
    init_allocator();
    // uart_sendline("shell3\r\n");
    irqtask_init_list();
    // uart_sendline("shell4\r\n");
    timer_list_init();
    // uart_sendline("shell5\r\n");

    init_thread_sched();
    // uart_sendline("shell6\r\n");

    init_rootfs();
    // uart_sendline("shell7\r\n");

    uart_interrupt_enable();
    // uart_sendline("shell8\r\n");

    el1_interrupt_enable();  // enable interrupt in EL1 -> EL1
    // uart_sendline("shell9\r\n");
    core_timer_enable();
    // uart_sendline("shell10\r\n");
    shell();
}
