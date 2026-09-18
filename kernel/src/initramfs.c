#include "initramfs.h"
#include "vfs.h"
#include "string.h"
#include "memory.h"
#include "cpio.h"
#include "uart1.h"
extern void* CPIO_DEFAULT_START;

struct file_operations initramfs_file_operations = {initramfs_write, initramfs_read, initramfs_open, initramfs_close, initramfs_lseek64,initramfs_getsize};
struct vnode_operations initramfs_vnode_operations = {initramfs_lookup, initramfs_create, initramfs_mkdir};

int register_initramfs() {
    struct filesystem fs;
    fs.name = "initramfs";
    fs.setup_mount = initramfs_setup_mount;
    return register_filesystem(&fs);
}

int initramfs_setup_mount(struct filesystem *fs, struct mount *_mount) {
    _mount->fs = fs;
    _mount->root = initramfs_create_vnode(0, dir_t);
    
    // add all file in initramfs.cpio to initramfs
    struct initramfs_inode *ramdir_inode = _mount->root->internal;
    char *filepath;
    char *filedata;
    unsigned int filesize;
    struct cpio_header *header_pointer = CPIO_DEFAULT_START;
    int idx = 0;

    while (header_pointer != 0) {
        int error = cpio_newc_parse_header(header_pointer, &filepath, &filesize, &filedata, &header_pointer);
        if (error) {
            uart_sendline("%s", "error\r\n");
            break;
        }

        // TRAILER!!! (last of file) will have header_pointer == 0
        if (header_pointer != 0) {
            // suppose all files are "file" type, not "dir" type
            struct vnode * filevnode = initramfs_create_vnode(0, file_t);
            struct initramfs_inode *fileinode = filevnode->internal;
            fileinode->data = filedata;
            fileinode->datasize = filesize;
            fileinode->name = filepath;
            ramdir_inode->entry[idx++] = filevnode;
        }
    }
    return 0;
}

struct vnode *initramfs_create_vnode(struct mount *_mount, enum fsnode_type type) {
    // vnode
    struct vnode *v = kmalloc(sizeof(struct vnode));
    v->f_ops = &initramfs_file_operations;
    v->v_ops = &initramfs_vnode_operations;
    v->mount = _mount;

    // initramfs node, which is pointed by vnode
    struct initramfs_inode *inode = kmalloc(sizeof(struct initramfs_inode));
    memset(inode, 0, sizeof(struct initramfs_inode));
    inode->type = type;
    inode->data = kmalloc(0x1000);
    v->internal = inode;
    return v;
}

// file operations
int initramfs_write(struct file *file, const void *buf, size_t len) { // initramfs is read only
    return -1;
}

int initramfs_read(struct file *file, void *buf, size_t len) {
    struct initramfs_inode *inode = file->vnode->internal;

    if (len + file->f_pos > inode->datasize) { // prevent read out of bound
        len = inode->datasize - file->f_pos;
        memcpy(buf, inode->data + file->f_pos, len);
        file->f_pos += len;
        return len;
    }
    else {
        memcpy(buf, inode->data + file->f_pos, len);
        file->f_pos += len;
        return len;
    }
    return -1;
}

int initramfs_open(struct vnode *file_node, struct file **target) {
    // file handle target is created in vfs_open
    (*target)->vnode = file_node;
    (*target)->f_ops = file_node->f_ops;
    (*target)->f_pos = 0;
    return 0;
}

int initramfs_close(struct file *file) {
    kfree(file);
    return 0;
}

long initramfs_lseek64(struct file *file, long offset, int whence) {
    if (whence == SEEK_SET) {
        file->f_pos = offset;
        return file->f_pos;
    }
    return -1;
}

// vnode operations
int initramfs_lookup(struct vnode *dir_node, struct vnode **target, const char *component_name) {
    struct initramfs_inode *dir_inode = dir_node->internal;

    int child_idx = 0;
    for (; child_idx < INITRAMFS_MAX_DIR_ENTRY; child_idx++) {
        struct vnode *vnode = dir_inode->entry[child_idx];
        if (!vnode) { // because we create in a order manner, so there shouldn't be any non-continuous empty spot
            break;
        }

        struct initramfs_inode *inode = vnode->internal;
        if (strcmp(component_name, inode->name) == 0) {
            *target = vnode;
            // uart_sendline("[initramfs] initramfs lookup found ");
            // uart_sendline(component_name);
            // uart_sendline(" in ");
            // uart_sendline((char*)(dir_inode->name));
            // uart_sendline("\r\n");
            return 0;
        }
    }
    uart_sendline("[initramfs] initramfs lookup \"does not\" found ");
    uart_sendline((char*)component_name);
    uart_sendline(" in ");
    uart_sendline((char*)(dir_inode->name));
    uart_sendline("\r\n");
    return -1;
}

int initramfs_create(struct vnode *dir_node, struct vnode **target, const char *component_name) { // initramfs is read only
    return -1;
}

int initramfs_mkdir(struct vnode *dir_node, struct vnode **target, const char *component_name) { // initramfs is read only
    return -1;
}

long initramfs_getsize(struct vnode *vd) {
    struct initramfs_inode *inode = vd->internal;
    return inode->datasize;
}
