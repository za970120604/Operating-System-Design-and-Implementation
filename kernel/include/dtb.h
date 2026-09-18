#ifndef _DTB_H_
#define _DTB_H_

#define uint32_t unsigned int
#define uint64_t unsigned long long

#define FDT_MAGIC_NUMBER 0xD00DFEED
#define FDT_BEGIN_NODE 0x1
#define FDT_END_NODE 0x2
#define FDT_PROP 0x3
#define FDT_NOP 0x9
#define FDT_END 0x0

struct fdt_header {
    unsigned int magic;             // magic, This field shall contain the value 0xd00dfeed (big-endian).
    unsigned int totalsize;         // totalsize, This field shall contain the total size in bytes of the devicetree data structure. 
                                    // This size shall encompass all sections of the structure: the header, the memory reservation block, structure block and strings block, as well as any free space gaps between the blocks or after the final block.
    unsigned int off_dt_struct;     // off_dt_struct, This field shall contain the offset in bytes of the structure block (see section 5.4) from the beginning of the header.
    unsigned int off_dt_strings;    // off_dt_strings, This field shall contain the offset in bytes of the strings block (see section 5.5) from the beginning of the header.
    unsigned int off_mem_rsvmap;    // off_mem_rsvmap, This field shall contain the offset in bytes of the memory reservation block (see section 5.3) from the beginning of the header.
    unsigned int version;           // version, This field shall contain the version of the devicetree data structure. 
    unsigned int last_comp_version; // last_comp_version, This field shall contain the lowest version of the devicetree data structure with which the version used is backwards compatible.
    unsigned int boot_cpuid_phys;   // boot_cpuid_phys, This field shall contain the physical ID of the system’s boot CPU.
    unsigned int size_dt_strings;   // size_dt_strings, This field shall contain the length in bytes of the strings block section of the devicetree blob.
    unsigned int size_dt_struct;    // size_dt_struct, This field shall contain the length in bytes of the structure block section of the devicetree blob.
};

struct fdt_reserve_entry {
    uint64_t address;
    uint64_t size;
};

// typedef void (*dtb_callback)(uint32_t node_type, char *name, void *value, uint32_t name_size);

// uint32_t uint32_endian_big2lttle(uint32_t data);
// uint64_t uint64_endian_big2lttle(uint64_t data);

// void traverse_device_tree(void *base, dtb_callback callback);  //traverse dtb tree
// void dtb_callback_show_tree(uint32_t node_type, char *name, void *value, uint32_t name_size);
// void dtb_callback_initramfs(uint32_t node_type, char *name, void *value, uint32_t name_size);
void dtb_find_and_store_reserved_memory();


typedef void (*fdt_callback_t)(char *node_name, char *prop_name, void *prop_value, int prop_len, char* target_node_name, char* target_prop_name);
void fdt_traverse(fdt_callback_t , char*, char*);
void initramfs_callback(char *, char *, void *, int, char*, char* );
unsigned long get_fdt_end_address();

#endif