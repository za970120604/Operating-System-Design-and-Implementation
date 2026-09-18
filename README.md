# Operating-System-Design-and-Implementation

A micro-kernel implementation for the **Raspberry Pi 3B+**, built from bare metal — covering boot, memory management, multiprocessing, and a virtual file system. Runnable on **QEMU** or real hardware.

## Features

### 1. Relocatable Bootloader
Receives the kernel image over UART and loads it into memory before jumping to it. The bootloader itself is linked at one address and relocates to a separate region, freeing up the kernel's target load address (`0x80000`) for the actual kernel image.

### 2. Exception / Interrupt Handling
Sets up the exception vector table and handlers for synchronous exceptions and IRQs, enabling safe hardware interrupt handling (timers, UART, etc.) at the kernel level.

### 3. Buddy System Memory Management
A physical page allocator using the classic buddy algorithm — power-of-two block sizes, doubly-linked free lists per order, and buddy-merging on free — extended with a slab allocator on top for general-purpose `malloc`/`free`.

### 4. Multiprocess Support
Process abstraction with separate physical/virtual address spaces per process, enabling context switching and process isolation on top of the memory manager.

### 5. VFS / File System
A virtual file system layer supporting mount points and multiple underlying file system implementations (e.g. tmpfs), with mount-state tracking to avoid duplicate or inconsistent mounts.

## Running

- **QEMU**: emulated Raspberry Pi 3B+ target for fast iteration and debugging
- **Real hardware**: deployable to an actual Raspberry Pi 3B+ via UART kernel transfer
