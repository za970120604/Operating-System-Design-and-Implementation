#ifndef _CPIO_H_
#define _CPIO_H_

/*
    cpio format : https://manpages.ubuntu.com/manpages/bionic/en/man5/cpio.5.html
    We are using "newc" format
    header, file path, file data, header  ......
    header + file path (padding 4 bytes)
    file data (padding 4 bytes)  (max size 4gb)
*/

#define CPIO_NEWC_HEADER_MAGIC "070701"    // big endian constant, to check whether it is big endian or little endian

extern void* CPIO_DEFAULT_START;
extern void* CPIO_DEFAULT_END;

struct cpio_header
{
    char c_magic[6];            // fixed, "070701".
    char c_ino[8];
    char c_mode[8];
    char c_uid[8];
    char c_gid[8];
    char c_nlink[8];
    char c_mtime[8];
    char c_filesize[8];
    char c_devmajor[8];
    char c_devminor[8];
    char c_rdevmajor[8];
    char c_rdevminor[8];
    char c_namesize[8];
    char c_check[8];
};

int cpio_find_program(char *filename, unsigned int *filesize, char **data);
void cpio_cat(char *filename);
void cpio_ls();

int cpio_newc_parse_header(struct cpio_header *this_header_pointer,
    char **pathname, unsigned int *filesize, char **data,
    struct cpio_header **next_header_pointer);

#endif /* _CPIO_H_ */
