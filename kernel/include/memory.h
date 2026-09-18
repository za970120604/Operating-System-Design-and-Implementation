// added in 0324, AM 10:00, in order to try to figure out nested syscall_fork problem
#ifndef BUDDY_H
#define BUDDY_H

#include "bcm2837/rpi_mmu.h"
#include "utils.h"

#define FRAME_SIZE 4096
#define BUDDY_MEMORY_BASE   PHYS_TO_VIRT(0x0) 
#define TOTAL_FRAME 0x3C000
#define BELONGED_FRAME -1
#define ALLOCATED_FRAME -2 // by buddy system 
#define RESERVED_FRAME -3 // pre-allocated by kernel
#define UNINIT_FRAME -4

#define MAX_ORDER 20
#define BUDDY_NULL ((buddy_t*)0)
#define SLAB_NULL ((slab_t*)0)

typedef struct slab_t {
    int available ; // is this slab page frame descriptor available
    int slab_type;
    int slab_size;  // how large is the slab this page frame stores  
    unsigned long start_address;   // the page frame address  
    int total_slabs;     //  total slabs can store inside this page frame, given the slab_size
    int free_slabs;  // total free slab slot that can use in this page frame
    unsigned char bitmap[64]; // bitmap stores the information of whether each slab slot is used in this page frame
    struct slab_t* next; // points to next page frame descriptor thats stores the same slab size objects
    struct slab_t* prev;
} slab_t;

typedef struct buddy_t {
    int status;          // 與原始 frame_array 相同的狀態定義
    unsigned int index;  // 此 buddy 的 frame 索引
    struct buddy_t* next;  // 指向下一個同大小的 free buddy
    struct buddy_t* prev;  // 指向上一個同大小的 free buddy
    int allocated_order;

    slab_t* slab_ptr;
    int is_slab_page;
} buddy_t;

// 改為指針變量，用於動態分配
extern buddy_t* buddy_frame_array;  // 指向動態分配的內存幀描述符數組
extern buddy_t** free_frame_lists;  // 指向每個 order 的 free list 頭

#define SLAB_TYPE 10
#define MAX_SLAB_FRAME_COUNT TOTAL_FRAME
extern unsigned int slab_sizes[];
extern slab_t* slab_frame_array;    // 改為指針，用於動態分配
extern slab_t** slab_start_list;    // 改為指針，用於動態分配

// 函數聲明
int size2order(unsigned int request_size);
int size2chunk(unsigned int request_size);

void buddy_init();
int buddy_allocate(int order);
void buddy_free(int block_index);
unsigned long index2address (unsigned long block_index);
unsigned long address2index(unsigned long address);

void slab_init();
unsigned long slab_allocate(unsigned int size);
void slab_free(slab_t* slab_page, unsigned long address);

void memory_reserve(unsigned long begin_address, unsigned long end_address);
void* s_allocator(unsigned int size);
void init_allocator();

void* malloc_b(unsigned int size);
void free_b(void* ptr);
void* malloc_s(unsigned int size);
void free_s(void* ptr);

void* kmalloc(unsigned int size);
void  kfree(void *ptr);

int print_order(int order, int infoflag);
#endif // BUDDY_H
