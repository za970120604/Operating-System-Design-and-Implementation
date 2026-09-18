#include "memory.h"
#include "uart1.h"
#include "exception.h"
#include "dtb.h"
#include "cpio.h"
#include "mmu.h"

#define heap_limit PHYS_TO_VIRT(0x06000000)

buddy_t* buddy_frame_array; 
buddy_t** free_frame_lists;
unsigned int slab_sizes[SLAB_TYPE] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048};
slab_t* slab_frame_array;
slab_t** slab_start_list;

extern volatile char _kernel_start;
extern volatile char _stack_end;
extern volatile char _stack_top;

extern char  _heap_start;
static char* htop_ptr = &_heap_start;
int printflag = 0;

#ifdef DEBUG
    #define memory_sendline(fmt, args ...) uart_sendline(fmt, ##args)
#else
    #define memory_sendline(fmt, args ...) (void)0
#endif

// ------ Lab2 ------
void* s_allocator(unsigned int size) {
    // -> htop_ptr
    // htop_ptr + 0x02:  heap_block size
    // htop_ptr + 0x10 ~ htop_ptr + 0x10 * k:
    //            { heap_block }
    // -> htop_ptr

    // 0x10 for heap_block header
    char* r = htop_ptr + 0x10;
    // size paddling to multiple of 0x10
    size = 0x10 + size - size % 0x10;
    *(unsigned int*)(r - 0x8) = size;
    htop_ptr += size;
    return r;
}

void s_free(void* ptr) {
    // TBD
}

int size2order(unsigned int request_size) {
    if (request_size == 0) {
        return 0;
    }
    unsigned int adjusted_size = (request_size + FRAME_SIZE - 1) / FRAME_SIZE;

    int order = 0;
    while ((unsigned int)(1 << order) < adjusted_size) {
        order++;
    }

    return order;
}

int size2chunk (unsigned int request_size) {
    for (int i = 0; i < SLAB_TYPE; i++) {
        if (slab_sizes[i] >= request_size) {
            return slab_sizes[i];
        }
    }
    return -1;
}

void buddy_init() {
    for (int i = 0; i <= MAX_ORDER; i++) {
        free_frame_lists[i] = BUDDY_NULL;  
    }

    for (int i = 0; i < TOTAL_FRAME; i++) {
        buddy_frame_array[i].index = i;
        if (buddy_frame_array[i].status != RESERVED_FRAME) {
            buddy_frame_array[i].status = UNINIT_FRAME;
        }
        buddy_frame_array[i].next = BUDDY_NULL;
        buddy_frame_array[i].prev = BUDDY_NULL;
        buddy_frame_array[i].allocated_order = -1;

        buddy_frame_array[i].slab_ptr = SLAB_NULL;
        buddy_frame_array[i].is_slab_page = 0;
    }

    for (int curr_order = MAX_ORDER; curr_order >= 0; curr_order--) {
        int block_size = 1 << curr_order;
        for (int i = 0; i < TOTAL_FRAME; i += block_size) {
            if (buddy_frame_array[i].status != UNINIT_FRAME) {
                continue;
            }
            if (i + block_size > TOTAL_FRAME) {
                continue;
            }
            int all_free = 1;
            for (int j = 0; j < block_size; j++) {
                if (buddy_frame_array[i + j].status != UNINIT_FRAME) {
                    all_free = 0;
                    break;
                }
            }
            if (all_free) {
                buddy_frame_array[i].status = curr_order;
                buddy_frame_array[i].next = free_frame_lists[curr_order];
                buddy_frame_array[i].prev = BUDDY_NULL;
                if (free_frame_lists[curr_order] != BUDDY_NULL) {
                    free_frame_lists[curr_order]->prev = &buddy_frame_array[i];
                }
                free_frame_lists[curr_order] = &buddy_frame_array[i];
                for (int j = 1; j < block_size; j++) {
                    buddy_frame_array[i + j].status = BELONGED_FRAME;
                }
            }
        }
    }
}

int buddy_allocate (int order) {
    int i;
    if (order > MAX_ORDER) {
        uart_sendline("[!] Buddy allocate fail !! size too large ...\r\n");
        return -1;
    }

    for (i = order; i <= MAX_ORDER; i++) {
        if (free_frame_lists[i] != BUDDY_NULL) {
            buddy_t* block = free_frame_lists[i];
            unsigned int block_index = block->index;

            free_frame_lists[i] = block->next;
            if (block->next != BUDDY_NULL) {
                block->next->prev = BUDDY_NULL;
            }

            if (printflag) {
                /* log start */
                char int_str[20];
                uart_sendline("[-] Remove page ");
                utils_int_to_str(block_index, int_str);
                uart_sendline(int_str);
                uart_sendline(" from order ");
                utils_int_to_str(i, int_str);
                uart_sendline(int_str);
                uart_sendline(". Range of pages: [");
                utils_int_to_str(block_index, int_str);
                uart_sendline(int_str);
                uart_sendline(", ");
                utils_int_to_str(block_index + (1 << i) - 1, int_str);
                uart_sendline(int_str);
                uart_sendline("]\r\n");
                /* log end */
            }

            int current_order = i;
            
            while (current_order > order) {
                current_order--;
                int buddy_index = block_index ^ (1 << current_order); 
                if (buddy_index < block_index) {
                    uart_sendline("[!] Corrupted list structure !!\r\n");
                }
                
                buddy_frame_array[buddy_index].status = current_order;
                buddy_frame_array[buddy_index].next = free_frame_lists[current_order];
                buddy_frame_array[buddy_index].prev = BUDDY_NULL;
                
                if (free_frame_lists[current_order] != BUDDY_NULL) {
                    free_frame_lists[current_order]->prev = &buddy_frame_array[buddy_index];
                }
                
                free_frame_lists[current_order] = &buddy_frame_array[buddy_index];
                if (printflag) {
                    char int_str[20];
                    /* log start */
                    uart_sendline("[+] Add page ");
                    utils_int_to_str(buddy_index, int_str);
                    uart_sendline(int_str);
                    uart_sendline(" to order ");
                    utils_int_to_str(current_order, int_str);
                    uart_sendline(int_str);
                    uart_sendline(". Range of pages: [");
                    utils_int_to_str(buddy_index, int_str);
                    uart_sendline(int_str);
                    uart_sendline(", ");
                    utils_int_to_str(buddy_index + (1 << current_order) - 1, int_str);
                    uart_sendline(int_str);
                    uart_sendline("]\r\n");
                    /* log end */
                }
            }

            buddy_frame_array[block_index].status = ALLOCATED_FRAME;
            buddy_frame_array[block_index].next = BUDDY_NULL;
            buddy_frame_array[block_index].prev = BUDDY_NULL;
            buddy_frame_array[block_index].allocated_order = order;
            
            return block_index; 
        }
    }
    uart_sendline("[!] Buddy allocate fail !! size too large...\r\n");
    return -1;
}

void buddy_free (int block_index) {
    if (buddy_frame_array[block_index].status != ALLOCATED_FRAME) {
        uart_sendline("[!] Buddy free fail !! try to free an unallocated block...\r\n");
        return;
    }
    int order = buddy_frame_array[block_index].allocated_order;
    buddy_frame_array[block_index].status = order; 
    int buddy_index = block_index ^ (1 << order);
    
    while (order < MAX_ORDER && 
           buddy_index < TOTAL_FRAME && 
           buddy_frame_array[buddy_index].status == order) {

        if (printflag) {
            /* log start */
            char int_str[20];
            uart_sendline("[-] Remove page ");
            utils_int_to_str(buddy_index, int_str);
            uart_sendline(int_str);
            uart_sendline(" from order ");
            utils_int_to_str(order, int_str);
            uart_sendline(int_str);
            uart_sendline(". Range of pages: [");
            utils_int_to_str(buddy_index, int_str);
            uart_sendline(int_str);
            uart_sendline(", ");
            utils_int_to_str(buddy_index + (1 << order) - 1, int_str);
            uart_sendline(int_str);
            uart_sendline("]\r\n");
            /* log end */
        }
        
        if (free_frame_lists[order] == &buddy_frame_array[buddy_index]) {
            free_frame_lists[order] = buddy_frame_array[buddy_index].next;
            if (buddy_frame_array[buddy_index].next != BUDDY_NULL) {
                buddy_frame_array[buddy_index].next->prev = BUDDY_NULL;
            }
        } 
        else {
            if (buddy_frame_array[buddy_index].prev != BUDDY_NULL) {
                buddy_frame_array[buddy_index].prev->next = buddy_frame_array[buddy_index].next;
                if (buddy_frame_array[buddy_index].next != BUDDY_NULL) {
                    buddy_frame_array[buddy_index].next->prev = buddy_frame_array[buddy_index].prev;
                }
            }
            else{
                uart_sendline("[!] Corrupted list structure !!\r\n");
            }
        }

        if (printflag) {    
            char int_str[20];
            uart_sendline("[@] Merge Pages ");
            if (buddy_index < block_index) {
                utils_int_to_str(buddy_index, int_str);
                uart_sendline(int_str);
                uart_sendline("-");
                utils_int_to_str(buddy_index + (1 << order) - 1, int_str);
                uart_sendline(int_str);
                uart_sendline(" and ");
                utils_int_to_str(block_index, int_str);
                uart_sendline(int_str);
                uart_sendline("-");
                utils_int_to_str(block_index + (1 << order) - 1, int_str);
                uart_sendline(int_str);
                uart_sendline(" merged into order ");
            } 
            else {
                utils_int_to_str(block_index, int_str);
                uart_sendline(int_str);
                uart_sendline("-");
                utils_int_to_str(block_index + (1 << order) - 1, int_str);
                uart_sendline(int_str);
                uart_sendline(" and ");
                utils_int_to_str(buddy_index, int_str);
                uart_sendline(int_str);
                uart_sendline("-");
                utils_int_to_str(buddy_index + (1 << order) - 1, int_str);
                uart_sendline(int_str); 
                uart_sendline(" merged into order ");
            }
            utils_int_to_str(order + 1, int_str);
            uart_sendline(int_str);
            uart_sendline(" block.\r\n");
        }

        if (buddy_index < block_index) {
            // 大索引被標記為 BELONGED_FRAME 並清除指針
            buddy_frame_array[block_index].status = BELONGED_FRAME;
            buddy_frame_array[block_index].next = BUDDY_NULL;
            buddy_frame_array[block_index].prev = BUDDY_NULL;
            buddy_frame_array[block_index].allocated_order = -1;
            
            // 更新 block_index 為較小的索引
            block_index = buddy_index;
        } 
        else {
            // 大索引被標記為 BELONGED_FRAME 並清除指針
            buddy_frame_array[buddy_index].status = BELONGED_FRAME;
            buddy_frame_array[buddy_index].next = BUDDY_NULL;
            buddy_frame_array[buddy_index].prev = BUDDY_NULL;
            buddy_frame_array[buddy_index].allocated_order = -1;
        }

        order++;
        buddy_index = block_index ^ (1 << order);
    }

    buddy_frame_array[block_index].status = order;

    if (printflag) {
        /* log start */
        char int_str[20];
        uart_sendline("[+] Add page ");
        utils_int_to_str(block_index, int_str);
        uart_sendline(int_str);
        uart_sendline(" to order ");
        utils_int_to_str(order, int_str);
        uart_sendline(int_str);
        uart_sendline(". Range of pages: [");
        utils_int_to_str(block_index, int_str);
        uart_sendline(int_str);
        uart_sendline(", ");
        utils_int_to_str(block_index + (1 << order) - 1, int_str);
        uart_sendline(int_str);
        uart_sendline("]\r\n");
        /* log end */
    }

    buddy_frame_array[block_index].next = free_frame_lists[order];
    buddy_frame_array[block_index].prev = BUDDY_NULL;
    
    if (free_frame_lists[order] != BUDDY_NULL) {
        free_frame_lists[order]->prev = &buddy_frame_array[block_index];
    }
    
    free_frame_lists[order] = &buddy_frame_array[block_index];
}

void slab_init() {
    for (int i = 0 ; i < SLAB_TYPE; i++){
        slab_start_list[i] = SLAB_NULL;
    }

    for(int i = 0; i < MAX_SLAB_FRAME_COUNT; i++){
        slab_frame_array[i].available = 1;
        slab_frame_array[i].slab_type = -1;
        slab_frame_array[i].slab_size = -1;
        slab_frame_array[i].start_address = -1;
        slab_frame_array[i].total_slabs = -1;
        slab_frame_array[i].free_slabs = -1;
        for (int j = 0; j < 64; j++) {  
            slab_frame_array[i].bitmap[j] = 0;  
        }
        slab_frame_array[i].next = SLAB_NULL;
        slab_frame_array[i].prev = SLAB_NULL;
    }
}

unsigned long slab_allocate(unsigned int size) {
    int slab_size = -1;
    int slab_type = -1;

    for (int i = 0; i < SLAB_TYPE; i++) {
        if (slab_sizes[i] >= size) {
            slab_size = slab_sizes[i];
            slab_type = i;
            break;
        }
    }

    if (slab_size == -1) {
        return -1; 
    }

    slab_t* slab_page = slab_start_list[slab_type];
    while (slab_page != SLAB_NULL) {
        if (slab_page->free_slabs > 0) {
            for (int i = 0; i < slab_page->total_slabs; i++) {
                int bitmap_index = i / 8; 
                int bit_pos = i % 8;  

                if ((slab_page->bitmap[bitmap_index] & (1UL << bit_pos)) == 0) {
                    slab_page->bitmap[bitmap_index] |= (1UL << bit_pos); 
                    slab_page->free_slabs--;
                    return (unsigned long)(slab_page->start_address + i * slab_size);
                }
            }
        }
        slab_page = slab_page->next;
    }

    int block_index = buddy_allocate(0);
    if (block_index == -1) {
        return -1; 
    }
    unsigned long address = index2address(block_index);

    slab_t* new_slab_frame;

#if MAX_SLAB_FRAME_COUNT < TOTAL_FRAME
    for (int i = 0; i < MAX_SLAB_FRAME_COUNT; i++) {
        if (slab_frame_array[i].available == 1 && slab_frame_array[i].slab_size == -1) {
            new_slab_frame = &slab_frame_array[i];
            break;
        }
    }
#else
    if (slab_frame_array[block_index].available != 1 || slab_frame_array[block_index].slab_size != -1) {
        uart_sendline("[!] Corrupted list structure !!\r\n");
    }
    new_slab_frame = &slab_frame_array[block_index];
#endif

    if (new_slab_frame == SLAB_NULL) {
        buddy_free(block_index);
        return -1; 
    }

    buddy_frame_array[block_index].slab_ptr = new_slab_frame;
    buddy_frame_array[block_index].is_slab_page = 1;

    new_slab_frame->available = 0;
    new_slab_frame->slab_type = slab_type;
    new_slab_frame->slab_size = slab_size;
    new_slab_frame->start_address = address;
    new_slab_frame->total_slabs = FRAME_SIZE / slab_size;
    new_slab_frame->free_slabs = new_slab_frame->total_slabs;
    for (int i = 0; i < 64; i++) { 
        new_slab_frame->bitmap[i] = 0;
    }

    new_slab_frame->prev = SLAB_NULL;
    new_slab_frame->next = slab_start_list[slab_type];
    if (slab_start_list[slab_type] != SLAB_NULL) {
        slab_start_list[slab_type]->prev = new_slab_frame;
    }
    slab_start_list[slab_type] = new_slab_frame;

    new_slab_frame->bitmap[0] |= 0x01;  // 分配第一個 slab
    new_slab_frame->free_slabs--;  // 減少空閒 slab 數量

    return (unsigned long)(new_slab_frame->start_address);
}

void slab_free (slab_t* slab_page, unsigned long address) {
    if (slab_page == SLAB_NULL || !(address >= slab_page->start_address && address < slab_page->start_address + FRAME_SIZE)) {
        uart_sendline("[!] Slab free fail !!\r\n");
        return;
    }

    unsigned long offset = address - slab_page->start_address;
    int slab_size = slab_page->slab_size;
    
    if (offset % slab_size != 0) {
        uart_sendline("[!] Unaligned slab address !!\r\n");
        return;
    }
    
    int index = offset / slab_size;
    if (index >= slab_page->total_slabs) {
        uart_sendline("[!] Slab index out of range\r\n");
        return;
    }
    
    int bitmap_index = index / 8;
    int bit_pos = index % 8;
    
    if ((slab_page->bitmap[bitmap_index] & (1UL << bit_pos)) == 0) {
        uart_sendline("[!] Freeing unallocated slab\r\n");
        return;
    }
    
    slab_page->bitmap[bitmap_index] &= ~(1UL << bit_pos);
    slab_page->free_slabs++;
            
    if (slab_page->free_slabs == slab_page->total_slabs) {
        int block_index = address2index(slab_page->start_address);
        
        buddy_frame_array[block_index].slab_ptr = SLAB_NULL;
        buddy_frame_array[block_index].is_slab_page = 0;
        buddy_free(block_index);

        if (slab_start_list[slab_page->slab_type] == slab_page) {
            slab_start_list[slab_page->slab_type] = slab_page->next;
            if (slab_page->next != SLAB_NULL) {
                slab_page->next->prev = SLAB_NULL;
            }
        } 
        else {
            if (slab_page->prev == SLAB_NULL) {
                uart_sendline("[!] Corrupted list structure !!\r\n");
                return;
            }

            slab_page->prev->next = slab_page->next;

            if (slab_page->next != SLAB_NULL) {
                slab_page->next->prev = slab_page->prev;
            }
        }

        slab_page->available = 1;
        slab_page->slab_type = -1;
        slab_page->slab_size = -1;
        slab_page->start_address = -1;
        slab_page->total_slabs = -1;
        slab_page->free_slabs = -1;
        for (int j = 0; j < 64; j++) {
            slab_page->bitmap[j] = 0;
        }
        slab_page->next = SLAB_NULL;
        slab_page->prev = SLAB_NULL;
    }     
}

unsigned long index2address(unsigned long block_index) {
    return BUDDY_MEMORY_BASE + (block_index * FRAME_SIZE);
}

unsigned long address2index(unsigned long address) {
    return (address - BUDDY_MEMORY_BASE) / FRAME_SIZE;
}

void memory_reserve(unsigned long begin_address, unsigned long end_address) {

    /* log start */
    uart_sendline("[x] Reserve address [");
    uart_binary_to_hex_long(begin_address);
    uart_sendline(", ");
    uart_binary_to_hex_long(end_address);
    uart_sendline("). Range of pages: [");
    char int_str[20];
    utils_int_to_str(address2index(begin_address), int_str);
    uart_sendline(int_str);
    uart_sendline(", ");
    utils_int_to_str(address2index(end_address + FRAME_SIZE - 1), int_str);
    uart_sendline(int_str);
    uart_sendline(")\r\n");
    /* log end */

    int begin_block_index = address2index(begin_address);
    int end_block_index = address2index(end_address + FRAME_SIZE - 1);


    for (int i = begin_block_index; i < end_block_index; i++){
        if (i >= 0 && i < TOTAL_FRAME) {
            buddy_frame_array[i].status = RESERVED_FRAME;
        }
    }
}

void init_allocator() {
    buddy_frame_array = (buddy_t*)s_allocator(sizeof(buddy_t) * TOTAL_FRAME);
    if (!buddy_frame_array) {
        uart_sendline("Failed to allocate buddy_frame_array");
        return;
    }
    
    free_frame_lists = (buddy_t**)s_allocator(sizeof(buddy_t*) * (MAX_ORDER + 1));
    if (!free_frame_lists) {
        uart_sendline("Failed to allocate free_frame_lists");
        return;
    }
    
    slab_frame_array = (slab_t*)s_allocator(sizeof(slab_t) * MAX_SLAB_FRAME_COUNT);
    if (!slab_frame_array) {
        uart_sendline("Failed to allocate slab_frame_array");
        return;
    }
    
    slab_start_list = (slab_t**)s_allocator(sizeof(slab_t*) * SLAB_TYPE);
    if (!slab_start_list) {
        uart_sendline("Failed to allocate slab_start_list");
        return;
    }

    for (int i = 0; i < TOTAL_FRAME; i++) {
        buddy_frame_array[i].status = UNINIT_FRAME;
        buddy_frame_array[i].index = i;
        buddy_frame_array[i].next = BUDDY_NULL;
        buddy_frame_array[i].prev = BUDDY_NULL;
    }

    dtb_find_and_store_reserved_memory(); // find spin tables in dtb
    memory_reserve(PHYS_TO_VIRT(MMU_PGD_ADDR), PHYS_TO_VIRT(MMU_PTE_ADDR+0x2000)); // // PGD's page frame at 0x1000 // PUD's page frame at 0x2000 PMD 0x3000-0x5000
    memory_reserve((unsigned long long)&_kernel_start, (unsigned long long)heap_limit); // kernel
    memory_reserve((unsigned long long)&_stack_end, (unsigned long long)&_stack_top);  // heap & stack -> simple allocator
    memory_reserve((unsigned long long)CPIO_DEFAULT_START, (unsigned long long)CPIO_DEFAULT_END);

    uart_sendline("After memory_reserve()\r\n");
    buddy_init();
    uart_sendline("After buddy_init()\r\n");
    slab_init();
    uart_sendline("After slab_init()\r\n");
}

void* malloc_b(unsigned int size) {
    // disable_interrupt();
    int order = size2order(size);
    int block_index = buddy_allocate(order);
    // enable_interrupt();
    if (block_index == -1){
        // enable_interrupt();
        return NULL;
    }

    void* addr = (void*)index2address(block_index);
    
    /* log start */
    if (printflag) {
        char int_str[20];
        uart_sendline("[Page]  Allocate ");
        uart_binary_to_hex_long((unsigned long)addr);
        uart_sendline(" at order ");
        utils_int_to_str(order, int_str);
        uart_sendline(int_str);
        uart_sendline(", page ");
        utils_int_to_str(block_index, int_str);
        uart_sendline(int_str);
        uart_sendline(". Next address at order ");
        utils_int_to_str(order, int_str);
        uart_sendline(int_str);
        uart_sendline(": ");
        buddy_t* next_free = free_frame_lists[order];
        if (next_free != BUDDY_NULL) {
            uart_binary_to_hex_long(index2address(next_free->index));
        } else {
            uart_sendline("0x-1");
        }
        uart_sendline("\r\n");
    }
    /* log end */
    
    return addr;
}

void free_b(void* ptr) {
    // disable_interrupt();
    if (ptr == NULL) {
        return;
    }
    unsigned long addr = (unsigned long)ptr;
    int block_index = address2index(addr);

    if (block_index < TOTAL_FRAME && buddy_frame_array[block_index].status == ALLOCATED_FRAME) {
        int order = buddy_frame_array[block_index].allocated_order;
        
        /* log start */
        if (printflag) {
            char int_str[20];
            uart_sendline("[Page]  Free ");
            uart_binary_to_hex_long(addr);
            uart_sendline(" and add back to order ");
            utils_int_to_str(order, int_str);
            uart_sendline(int_str);
            uart_sendline(", page ");
            utils_int_to_str(block_index, int_str);
            uart_sendline(int_str);
            uart_sendline(". Next address at order ");
            utils_int_to_str(order, int_str);
            uart_sendline(int_str);
            uart_sendline(": ");
            
            // 檢查當前空閒列表的下一個地址
            buddy_t* next_free = free_frame_lists[order];
            if (next_free != BUDDY_NULL) {
                uart_binary_to_hex_long(index2address(next_free->index));
            } 
            else {
                uart_sendline("0x-1");
            }
            uart_sendline("\r\n");
        }
        /* log end */
        
        buddy_free(block_index);
        // enable_interrupt();
        return;
    }
    // enable_interrupt();
    uart_sendline("[!] Buddy free fail !!\r\n");
}

void* malloc_s(unsigned int size) {
    // disable_interrupt();
    
    unsigned long addr = slab_allocate(size);
    if (addr == -1) {
        // enable_interrupt();
        uart_sendline("[!] Slab allocate fail !!\r\n");
        return NULL;
    }
    
    /* log start */
    if (printflag) {
        char int_str[20];
        uart_sendline("[Chunk] Allocate ");
        uart_binary_to_hex_long((unsigned long)addr);
        uart_sendline(" at chunk size ");
        utils_int_to_str(size2chunk(size), int_str);
        uart_sendline(int_str);
        uart_sendline("\r\n");
    }
    /* log end */
    
    // enable_interrupt();
    return (void*)addr;
}

void free_s(void* ptr) {
    if (ptr == NULL) {
        uart_sendline("[!] Slab free failed !! try to free a NULL pointer...\r\n");
        return;
    }
    
    unsigned long addr = (unsigned long)ptr;
    int block_index = address2index(addr);
    
    if (block_index < TOTAL_FRAME && buddy_frame_array[block_index].is_slab_page == 1) {
        slab_t* slab_page = buddy_frame_array[block_index].slab_ptr;
        
        if (slab_page != SLAB_NULL) {
            /* log start */
            if (printflag) {
                char int_str[20];
                uart_sendline("[Chunk] Free ");
                uart_binary_to_hex_long((unsigned long)addr);
                uart_sendline(" at chunk size ");
                utils_int_to_str(slab_page->slab_size, int_str);
                uart_sendline(int_str);
                uart_sendline("\r\n");
            }
            /* log end */
            
            slab_free(slab_page, addr);
            return;
        }
    }
    uart_sendline("[!] Slab free failed !! try to free a not slab pointer...\r\n");
}

void* kmalloc(unsigned int size) {
    void* addr = NULL;
    disable_interrupt();
    if (size <= slab_sizes[SLAB_TYPE-1]) {
        addr = malloc_s(size);
    } 
    else {
        addr = malloc_b(size);
    }
    enable_interrupt(); 
    return addr;
}

void kfree(void* ptr) {
    if (ptr == NULL) {
        return;
    }
    disable_interrupt();
    unsigned long addr = (unsigned long)ptr;
    int block_index = address2index(addr);
    
    if (block_index < TOTAL_FRAME) {
        if (buddy_frame_array[block_index].is_slab_page) {
            free_s(ptr);
        } 
        else {
            free_b(ptr);
        }
    }
    else {
        uart_sendline("[!] Unknown ptr !!\r\n");
    }
    enable_interrupt();
}

int print_order(int order, int infoflag) {
    if (order > MAX_ORDER) {
        return 0;
    }
    char str[20];
    disable_interrupt();
    buddy_t* curr = free_frame_lists[order];
    int count = 0;
    while (curr != BUDDY_NULL) {
        if (0) {
            if (count > 0) uart_sendline(", ");
            utils_int_to_str(curr->index, str);
            uart_sendline(str);
        }
        count++;
        curr = curr->next;
    }
    enable_interrupt();
    
    if (infoflag) {
        uart_sendline("Order ");
        utils_int_to_str(order, str);
        uart_sendline(str);
        uart_sendline(": ");
        uart_sendline(" (");
        utils_int_to_str(count, str);
        uart_sendline(str);
        uart_sendline(")\r\n");
    }
    return count;
}