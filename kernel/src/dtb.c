#include "bcm2837/rpi_mmu.h"
#include "dtb.h"
#include "uart1.h"
#include "string.h"
#include "cpio.h"
#include "memory.h"
void* dtb_base = 0;

void dtb_find_and_store_reserved_memory() {
    struct fdt_header *header = (struct fdt_header *) dtb_base;
    if (utils_change_endian_32(header->magic) != FDT_MAGIC_NUMBER) {
        uart_sendline("test\r\n");
        uart_sendline("traverse_device_tree : wrong magic in traverse_device_tree");
        return;
    }

    // off_mem_rsvmap stores all of reserve memory map with address and size
    char *dt_mem_rsvmap_ptr = (char *)((char *)header + utils_change_endian_32(header->off_mem_rsvmap));
    struct fdt_reserve_entry *reverse_entry = (struct fdt_reserve_entry *)dt_mem_rsvmap_ptr;

    // reserve memory which is defined by dtb
    while (reverse_entry->address != 0 || reverse_entry->size != 0) {
        unsigned long long start = PHYS_TO_VIRT(utils_change_endian_64(reverse_entry->address));
        unsigned long long end = utils_change_endian_64(reverse_entry->size) + start;
        memory_reserve(start, end);
        reverse_entry++;
    }

    // reserve device tree itself
    memory_reserve((unsigned long long)dtb_base, (unsigned long long)dtb_base + utils_change_endian_32(header->totalsize));
}

void initramfs_callback(char *node_name, char *prop_name, void *prop_value, int prop_len, char* target_node_name, char* target_prop_name) {
    if (utils_string_compare(node_name, target_node_name) > 0 && utils_string_compare(prop_name, target_prop_name) > 0) {
        if (utils_string_compare("linux,initrd-start", target_prop_name) > 0) {
            unsigned long initrd_start = utils_change_endian_32(*(unsigned int *)prop_value);
            uart_sendline("Target Node: ");
            uart_sendline(target_node_name);
            uart_sendline(", Target device: ");
            uart_sendline(target_prop_name);
            uart_sendline(", physical address starts at: ");
            uart_binary_to_hex_long(initrd_start);
            uart_sendline("\r\n");
            CPIO_DEFAULT_START = (void *)(unsigned long long)PHYS_TO_VIRT(initrd_start);
        }
        else if (utils_string_compare("linux,initrd-end", target_prop_name) > 0) {
            unsigned long initrd_end = utils_change_endian_32(*(unsigned int *)prop_value);
            uart_sendline("Target Node: ");
            uart_sendline(target_node_name);
            uart_sendline(", Target device: ");
            uart_sendline(target_prop_name);
            uart_sendline(", physical address starts at: ");
            uart_binary_to_hex_long(initrd_end);
            uart_sendline("\r\n");
            CPIO_DEFAULT_END = (void *)(unsigned long long)PHYS_TO_VIRT(initrd_end);
        } 
    }
}

void fdt_traverse(fdt_callback_t callback, char* target_node_name, char* target_prop_name) {
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
    

    struct fdt_header *header = (struct fdt_header *)dtb_base;
    if (utils_change_endian_32(header->magic) != FDT_MAGIC_NUMBER) {
        uart_sendline("Invalid FDT magic number!\n");
        return;
    } 
    else {
        uart_sendline("Valid FDT magic number!\n");
    }

    /*
        The structure block is composed of a sequence of pieces, each beginning with a token, that is, a big-endian 32-bit integer. 
        Some tokens are followed by extra data, the format of which is determined by the token value. 
        All tokens shall be aligned on a 32-bit boundary, which may require padding bytes (with a value of 0x0) to be inserted after the previous token’s data.
    */
    unsigned int *dt_struct = (unsigned int *)((char *)dtb_base + utils_change_endian_32(header->off_dt_struct));
    char *dt_strings = (char *)dtb_base + utils_change_endian_32(header->off_dt_strings);
    char *current_node = NULL;

    while ((unsigned long)dt_struct < (unsigned long)dtb_base + utils_change_endian_32(header->totalsize)) {
        unsigned int token = utils_change_endian_32(*dt_struct++);

        if (token == FDT_BEGIN_NODE) {  
            current_node = (char *)dt_struct;
            /*
                The FDT_BEGIN_NODE token marks the beginning of a node’s representation. 
                It shall be followed by the node’s unit name as extra data. 
                The name is stored as a null-terminated string, and shall include the unit address (see section 2.2.1), if any. 
                The node name is followed by zeroed padding bytes, if necessary for alignment.
            */
            dt_struct = (unsigned int *)utils_align((unsigned long)dt_struct + utils_strlen(current_node) + 1, 4);
        } 
        else if (token == FDT_PROP) {
            /*
                The FDT_PROP token marks the beginning of the representation of one property in the devicetree. 
                It shall be followed by extra data describing the property. 
                This data consists first of the property’s length and name represented as the following C structure:
                    struct {
                        uint32_t len;
                        uint32_t nameoff;
                    }
                Both the fields in this structure are 32-bit big-endian integers.
                len gives the length of the property’s value in bytes (which may be zero, indicating an empty property, see section 2.2.4.2).
                nameoff gives an offset into the strings block (see section 5.5) at which the property’s name is stored as a null-terminated string.
                After this structure, the property’s value is given as a byte string of length len. 
                This value is followed by zeroed padding bytes (if necessary) to align to the next 32-bit boundary and then the next token.
            */
            unsigned int prop_len = utils_change_endian_32(*dt_struct++);
            unsigned int name_offset = utils_change_endian_32(*dt_struct++);
            char *prop_name = dt_strings + name_offset;
            void *prop_value = dt_struct;
            dt_struct = (unsigned int *)utils_align((unsigned long)dt_struct + prop_len, 4);
            
            if (callback) {
                callback(current_node, prop_name, prop_value, prop_len, target_node_name, target_prop_name);
            }
        } 
        else if (token == FDT_END_NODE) {  
            continue;
        } 
        else if (token == FDT_NOP) {  
            continue;
        } 
        else if (token == FDT_END) {  
            break;
        }
    }
}
