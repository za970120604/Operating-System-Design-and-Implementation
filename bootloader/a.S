.section ".text.boot"
.global _start

_start:
    mrs     x1, mpidr_el1 // mpidr_el1: Multiprocessor Affinity Register
    and     x1, x1, #3   // get cpu id and place it in x1 register
    mov     x20, x0 // save dtb base address to x20
    cbz     x1, _relocate // if cpu id is zero (main core), go to "_relocate_info", otherwise go to "_halt"

_halt:  
    wfe
    b       _halt // wait for event, halt (busy looping the slave core)

_relocate:
    ldr     x1, =_bootloader_start       // 源地址
    ldr     x2, =_bootloader_size        // 大小
    ldr     x3, =_relocation_address     // 目標地址 (使用鏈接腳本中的符號)

_relocate_loop:
    cmp     x2, #8                      // 檢查剩餘大小是否 >= 8
    blt     _relocate_remaining         // 如果小於8字節，處理剩餘字節

    ldr     x4, [x1], #8                // 從源地址加載8字節到x4並移動指針
    str     x4, [x3], #8                // 存儲8字節到目標地址並移動指針
    sub     x2, x2, #8                  // 減少大小計數器8字節
    b       _relocate_loop              // 繼續循環

_relocate_remaining:
    cbz     x2, _set                    // 如果沒有剩餘字節，跳轉到_set
    ldrb    w4, [x1], #1                // 從源地址加載1字節到w4並移動指針
    strb    w4, [x3], #1                // 存儲1字節到目標地址並移動指針
    sub     x2, x2, #1                  // 減少大小計數器1字節
    cbnz    x2, _relocate_remaining     // 繼續循環直到x1 == 0

_set: 
    // The stack grows downward in AArch64 (from high (0x80000) to low memory (0x79999, 0x79998) addresses).
    // We need to ensure that sp is set to a valid, writable memory region before using the stack.
    // It must be aligned properly according to AAPCS64 (Arm Procedure Call Standard), meaning sp should be 16-byte aligned.
    // since 0x80000 is 16-byte aligned, so we can directly use it as the stack pointer.
    ldr     x1, =_start  // load address of _kernel_boot to x1
    mov     sp, x1 // stack pointer before .text.boot section

    ldr     x1, =_bss_section_start
    ldr     w2, =_bss_size

_clear_bss:
    cmp     w2, #8
    blt     _clear_remaining   // If w2 < 8, handle the remaining bytes
    str     xzr, [x1], #8      // Clear 8 bytes
    sub     w2, w2, #8         // Decrease size
    b       _clear_bss         // Continue loop

_clear_remaining:
    cbz     w2, _bootloader_main_entry           // If nothing left, jump to main
    strb    wzr, [x1], #1      // Clear remaining 1 byte at a time
    sub     w2, w2, #1
    cbnz    w2, _clear_remaining  // Loop until w2 = 0

_bootloader_main_entry:  
    ldr     x0, =main
    ldr     x1, =_bootloader_start
    ldr     x2, =_relocation_address

    sub     x0, x0, x1
    add     x0, x0, x2
    
    br      x0      // br : branch to address stored in register
    b      _halt