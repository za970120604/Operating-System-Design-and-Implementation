#include "vfs.h"
#include "tmpfs.h"
#include "memory.h"
#include "string.h"
#include "uart1.h"
#include "initramfs.h"
#include "dev_uart.h"
#include "dev_framebuffer.h"

struct mount *rootfs;
struct filesystem reg_fs[MAX_FS_REG];
struct file_operations reg_dev[MAX_DEV_REG];

int register_filesystem(struct filesystem *fs) {
    for (int i = 0; i < MAX_FS_REG;i++) {
        if(!reg_fs[i].name) {
            reg_fs[i].name = fs->name;
            reg_fs[i].setup_mount = fs->setup_mount;
            return i;
        }
    }
    return -1;
}

int register_dev(struct file_operations *fo) {
    for (int i = 0; i < MAX_FS_REG; i++) {
        if (!reg_dev[i].open) {
            reg_dev[i] = *fo;
            return i;
        }
    }
    return -1;
}

struct filesystem* find_filesystem(const char* fs_name) {
    for (int i = 0; i < MAX_FS_REG; i++) {
        if ( strcmp(reg_fs[i].name,fs_name) ==0 ) {
            return &reg_fs[i];
        }
    }
    return 0;
}


// file operations
int vfs_open(const char *pathname, int flags, struct file **target) {
    struct vnode *node;

    //  Create a new file if O_CREAT is specified in flags and vnode not found
    if (vfs_lookup(pathname, &node) != 0 && (flags & O_CREAT)) {
        uart_sendline("[vfs] OCREATE flag is specified\r\n");

        int last_slash_idx = 0;
        for (int i = 0; i < strlen(pathname); i++) {
            if(pathname[i] == '/') {
                last_slash_idx = i;
            }
        }

        char dirname[MAX_PATH_NAME+1];
        strcpy(dirname, pathname);
        dirname[last_slash_idx] = 0;

        // update dirname to node
        if (vfs_lookup(dirname, &node)!= 0) {
            uart_sendline("cannot ocreate no dir name\r\n");
            return -1;
        }

        // create a new file node on node, &node is new file, 3rd arg is filename
        node->v_ops->create(node, &node, pathname+last_slash_idx+1);
        
        // create file handle
        *target = kmalloc(sizeof(struct file));
        
        // attach vnode with file handle
        node->f_ops->open(node, target);
        (*target)->flags = flags;
        return 0;
    }
    else {
        // create file handle
        *target = kmalloc(sizeof(struct file));

        // attach vnode with file handle
        node->f_ops->open(node, target);
        (*target)->flags = flags;
        return 0;
    }

    return -1;
}

int vfs_close(struct file *file) {
    file->f_ops->close(file);
    return 0;
}

int vfs_write(struct file *file, const void *buf, size_t len) {
    return file->f_ops->write(file,buf,len);
}

int vfs_read(struct file *file, void *buf, size_t len) {
    return file->f_ops->read(file, buf, len);
}

int vfs_mkdir(const char *pathname) {
    char dirname[MAX_PATH_NAME] = {};  // /a/b/c --> /a/b
    char newdirname[MAX_PATH_NAME] = {}; // /a/b/c --> c

    // search for last directory
    int last_slash_idx = 0;
    for (int i = 0; i < strlen(pathname); i++) {
        if (pathname[i] == '/') {
            last_slash_idx = i;
        }
    }
    memcpy(dirname, pathname, last_slash_idx);
    strcpy(newdirname, pathname + last_slash_idx + 1);

    // create directory if dirname is found
    struct vnode *node;
    if (vfs_lookup(dirname, &node) == 0) {
        // node is the old dir, &node is new dir
        node->v_ops->mkdir(node, &node, newdirname);
        return 0;
    }

    uart_sendline("[vfs] vfs_mkdir cannot find old directory\r\n");
    return -1;
}

int vfs_mount(const char *target, const char *filesystem) {
    struct vnode *dirnode;
    struct filesystem *fs = find_filesystem(filesystem);
    
    if (!fs) {
        uart_sendline("[vfs] vfs_mount cannot find filesystem\r\n");
        return -1;
    }
    
    if (vfs_lookup(target, &dirnode) == -1) {
        uart_sendline("[vfs] vfs_mount cannot find target directory\r\n");
        return -1;
    }
    else {
        dirnode->mount = kmalloc(sizeof(struct mount));
        fs->setup_mount(fs, dirnode->mount);
    }
    return 0;
}

int vfs_lookup(const char *pathname, struct vnode **target) {
    // uart_sendline("[vfs] vfs look up ");
    // uart_sendline((char*)pathname);
    // if (strlen(pathname) == 0) {
    //     uart_sendline("/");
    // }
    // uart_sendline("\r\n");

    // if no path input, return root
    if (strlen(pathname) == 0) {
        *target = rootfs->root;
        return 0;
    }

    struct vnode *dirnode = rootfs->root;
    char component_name[MAX_FILE_NAME+1] = {};
    int c_idx = 0;
    
    // iterate through directory, e.g: lookup "a/b/c"
    /*
        use rootfs->lookup find vnode of a
        use a->lookup find vnode of b
        use b->lookup find vnode of c
    */
    for (int i = 1; i < strlen(pathname); i++) {
        if (pathname[i] == '/') {
            component_name[c_idx++] = 0;
            // use parent vnode lookup to file vnode
            if (dirnode->v_ops->lookup(dirnode, &dirnode, component_name) != 0) {
                return -1;
            }
            
            // redirect to mounted filesystem
            while (dirnode->mount){
                dirnode = dirnode->mount->root;
            }
            c_idx = 0;
        }
        else{
            component_name[c_idx++] = pathname[i];
        }
    }

    // handle last slash component
    component_name[c_idx++] = 0;
    // use parent vnode lookup to file vnode
    if (dirnode->v_ops->lookup(dirnode, &dirnode, component_name) != 0) {
        return -1;
    }
    // redirect to mounted filesystem
    while (dirnode->mount){
        dirnode = dirnode->mount->root;
    }

    *target = dirnode;
    return 0;
}

int vfs_mknod(char* pathname, int id) {
    struct file* f = kmalloc(sizeof(struct file));
    // create leaf and its file operations
    vfs_open(pathname, O_CREAT, &f);
    f->vnode->f_ops = &reg_dev[id];
    vfs_close(f);
    return 0;
}

void init_rootfs() {
    // tmpfs
    int idx = register_tmpfs();
    rootfs = kmalloc(sizeof(struct mount));
    reg_fs[idx].setup_mount(&reg_fs[idx], rootfs);

    // initramfs
    vfs_mkdir("/initramfs");
    register_initramfs();
    vfs_mount("/initramfs","initramfs");

    // dev_fs
    vfs_mkdir("/dev");
    int uart_id = init_dev_uart();
    vfs_mknod("/dev/uart", uart_id);
    int framebuffer_id = init_dev_framebuffer();
    vfs_mknod("/dev/framebuffer", framebuffer_id);
}

// In syscall, filepath is relative path, so we need to combine relative path and cwd to get absolute path
// char *get_absolute_path(char *path, char *curr_working_dir) {
//     // if relative path -> add root path
//     if(path[0] != '/') {
//         char tmp[MAX_PATH_NAME];
//         strcpy(tmp, curr_working_dir);
//         if(strcmp(curr_working_dir,"/") != 0) {
//             strcat(tmp, "/");
//         }
//         strcat(tmp, path);
//         strcpy(path, tmp);
//     }

//     char absolute_path[MAX_PATH_NAME+1] = {};
    
//     int idx = 0;
//     for (int i = 0; i < strlen(path); i++) {
//         // trim /..
//         if (path[i] == '/' && path[i+1] == '.' && path[i+2] == '.') {
//             for (int j = idx; j >= 0; j--) {
//                 if(absolute_path[j] == '/') {
//                     absolute_path[j] = 0;
//                     idx = j;
//                 }
//             }
//             i += 2;
//             continue;
//         }

//         // ignore /.
//         if (path[i] == '/' && path[i+1] == '.') {
//             i++;
//             continue;
//         }

//         absolute_path[idx++] = path[i];
//     }
//     absolute_path[idx] = 0;

//     return strcpy(path, absolute_path);
// }

char *get_absolute_path(char *path, char *curr_working_dir) {
    char temp_path[MAX_PATH_NAME + 1];
    
    // Step 1: Convert relative path to absolute path
    if (path[0] != '/') {
        // Check if concatenation would overflow
        if (strlen(curr_working_dir) + strlen(path) + 2 > MAX_PATH_NAME) {
            // Path too long, return original
            return path;
        }
        
        strcpy(temp_path, curr_working_dir);
        
        // Add separator if current directory doesn't end with '/'
        if (strcmp(curr_working_dir, "/") != 0 && 
            curr_working_dir[strlen(curr_working_dir) - 1] != '/') {
            strcat(temp_path, "/");
        }
        
        strcat(temp_path, path);
    } else {
        // Already absolute path
        if (strlen(path) > MAX_PATH_NAME) {
            return path;
        }
        strcpy(temp_path, path);
    }
    
    // Step 2: Resolve . and .. components
    char absolute_path[MAX_PATH_NAME + 1];
    int idx = 0;
    int len = strlen(temp_path);
    
    for (int i = 0; i < len; i++) {
        // Handle "/./" - current directory reference
        if (i + 2 < len && temp_path[i] == '/' && 
            temp_path[i + 1] == '.' && temp_path[i + 2] == '/') {
            // Skip the "/." part, continue with the next '/'
            i += 1;
            continue;
        }
        
        // Handle "/." at the end of path
        if (i + 1 < len && temp_path[i] == '/' && 
            temp_path[i + 1] == '.' && i + 2 == len) {
            // Skip the "." at the end
            i += 1;
            continue;
        }
        
        // Handle "/../" - parent directory reference
        if (i + 3 < len && temp_path[i] == '/' && 
            temp_path[i + 1] == '.' && temp_path[i + 2] == '.' && 
            temp_path[i + 3] == '/') {
            // Go back to previous directory
            if (idx > 0) {
                // Remove characters until we find the previous '/'
                idx--;
                while (idx > 0 && absolute_path[idx] != '/') {
                    idx--;
                }
            }
            // Skip the "/.." part
            i += 2;
            continue;
        }
        
        // Handle "/.." at the end of path
        if (i + 2 < len && temp_path[i] == '/' && 
            temp_path[i + 1] == '.' && temp_path[i + 2] == '.' && 
            i + 3 == len) {
            // Go back to previous directory
            if (idx > 0) {
                // Remove characters until we find the previous '/'
                idx--;
                while (idx > 0 && absolute_path[idx] != '/') {
                    idx--;
                }
            }
            // Skip the ".." at the end
            i += 2;
            continue;
        }
        
        // Normal character - copy it
        if (idx < MAX_PATH_NAME) {
            absolute_path[idx++] = temp_path[i];
        }
    }
    
    // Null terminate
    absolute_path[idx] = '\0';
    
    // Handle empty result (went above root)
    if (idx == 0) {
        absolute_path[0] = '/';
        absolute_path[1] = '\0';
    }
    
    // Copy result back to original buffer
    strcpy(path, absolute_path);
    return path;
}

