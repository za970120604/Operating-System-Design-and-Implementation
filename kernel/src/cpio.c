#include "cpio.h"
#include "dtb.h"
#include "string.h"
#include "utils.h"
#include "uart1.h"

void* CPIO_DEFAULT_START;
void* CPIO_DEFAULT_END;

char* find_file(char *name) {
    char *addr = (char *) CPIO_DEFAULT_START;
    while (utils_string_compare((char *)(addr + sizeof(struct cpio_header)), "TRAILER!!!") == 0) {
        struct cpio_header *header = (struct cpio_header *) addr;
        unsigned long filename_size = utils_HexStr2Int(header->c_namesize, sizeof(header->c_namesize));
        unsigned long file_size = utils_HexStr2Int(header->c_filesize, sizeof(header->c_filesize));
        unsigned long header_pathname_size = sizeof(struct cpio_header) + filename_size;

        header_pathname_size = utils_align(header_pathname_size, 4);
        file_size = utils_align(file_size, 4);

        if (utils_string_compare((char *)(addr + sizeof(struct cpio_header)), name) != 0) {
            return addr;
        }
        addr += (header_pathname_size + file_size);
    }
    return (void*)0;
}

void cpio_ls() {
    char *addr = (char *) CPIO_DEFAULT_START;
    while (utils_string_compare((char *)(addr + sizeof(struct cpio_header)), "TRAILER!!!") == 0) {
        struct cpio_header *header = (struct cpio_header *) addr;
        unsigned long filename_size = utils_HexStr2Int(header->c_namesize, sizeof(header->c_namesize));
        unsigned long file_size = utils_HexStr2Int(header->c_filesize, sizeof(header->c_filesize));
        unsigned long header_pathname_size = sizeof(struct cpio_header) + filename_size;

        header_pathname_size = utils_align(header_pathname_size, 4);
        file_size = utils_align(file_size, 4);

        uart_sendline(addr + sizeof(struct cpio_header));
        uart_sendline("\r\n");

        addr += (header_pathname_size + file_size);
    }
}

void cpio_cat(char *filename) {
    char *target = find_file(filename);
    if (target) {
        struct cpio_header *header = (struct cpio_header *) target;
        unsigned long filename_size = utils_HexStr2Int(header->c_namesize, sizeof(header->c_namesize));
        unsigned long file_size = utils_HexStr2Int(header->c_filesize, sizeof(header->c_filesize));
        unsigned long header_pathname_size = sizeof(struct cpio_header) + filename_size;

        header_pathname_size = utils_align(header_pathname_size, 4);
        file_size = utils_align(file_size, 4);

        char *file_content = target + header_pathname_size;
        for (unsigned int i = 0; i < file_size; i++) {
            uart_send(file_content[i]);
        }
        // uart_send_string("\n");
    } 
    else {
        uart_sendline("File not found\r\n");
    }
}

int cpio_find_program(char *filename, unsigned int *filesize, char **data) {
    char *target = find_file(filename);
    if (target) {
        struct cpio_header *header = (struct cpio_header *) target;
        unsigned long filename_size = utils_HexStr2Int(header->c_namesize, sizeof(header->c_namesize));
        unsigned long original_file_size = utils_HexStr2Int(header->c_filesize, sizeof(header->c_filesize));
        unsigned long header_pathname_size = sizeof(struct cpio_header) + filename_size;

        header_pathname_size = utils_align(header_pathname_size, 4);
        unsigned int file_size = utils_align(original_file_size, 4);
        char *file_content = target + header_pathname_size;
        *filesize = file_size;
        *data = file_content;
        return 1;
    } 
    else {
        uart_sendline("File not found\r\n");
        return 0;
    }
}

static unsigned int parse_hex_str(char *s, unsigned int max_len)
{
    unsigned int r = 0;
    for (unsigned int i = 0; i < max_len; i++) {
        r *= 16;
        if      (s[i] >= '0' && s[i] <= '9') r += s[i] - '0';
        else if (s[i] >= 'a' && s[i] <= 'f') r += s[i] - 'a' + 10;
        else if (s[i] >= 'A' && s[i] <= 'F') r += s[i] - 'A' + 10;
        else return r;
    }
    return r;
}

int cpio_newc_parse_header(struct cpio_header *this_header_pointer, char **pathname, unsigned int *filesize, char **data, struct cpio_header **next_header_pointer)
{
    if (strncmp(this_header_pointer->c_magic, CPIO_NEWC_HEADER_MAGIC, sizeof(this_header_pointer->c_magic)) != 0) return -1;

    *filesize = parse_hex_str(this_header_pointer->c_filesize,8);
    *pathname = ((char *)this_header_pointer) + sizeof(struct cpio_header); 

    unsigned int pathname_length = parse_hex_str(this_header_pointer->c_namesize,8);
    unsigned int offset = pathname_length + sizeof(struct cpio_header);
    offset = offset % 4 == 0 ? offset:(offset+4-offset%4);
    *data = (char *)this_header_pointer+offset;

    if(*filesize==0)
    {
        *next_header_pointer = (struct cpio_header*)*data;
    }
    else
    {
        offset = *filesize;
        *next_header_pointer = (struct cpio_header*)(*data + (offset%4==0?offset:(offset+4-offset%4)));
    }
    if(strncmp(*pathname,"TRAILER!!!", sizeof("TRAILER!!!"))==0) *next_header_pointer = 0;

    return 0;
}


