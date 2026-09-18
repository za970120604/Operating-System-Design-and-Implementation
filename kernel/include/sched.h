#ifndef _SCHED_H_
#define _SCHED_H_

#include "utils.h"
#include "vfs.h"

#define THREAD_FREE     0
#define THREAD_ACTIVE   1
#define THREAD_ZOMBIE   2

#define MAX_THREAD_COUNT    32768
#define USTACK_SIZE         0x4000
#define KSTACK_SIZE         0x4000
#define SIGNAL_MAX          64

extern void switch_to(void *curr_context, void *next_context);
extern void store_context(void *curr_context);
extern void load_context(void *curr_context);
extern void *get_current();

typedef struct thread_context
{
    unsigned long x19;
    unsigned long x20;
    unsigned long x21;
    unsigned long x22;
    unsigned long x23;
    unsigned long x24;
    unsigned long x25;
    unsigned long x26;
    unsigned long x27;
    unsigned long x28;
    unsigned long fp;
    unsigned long lr;
    unsigned long sp;
    void* pgd;   // use for MMU mapping (user space)
} thread_context_t;

typedef struct thread
{
    thread_context_t  context;
    char*             data;
    unsigned int      datasize;
    /////////////////////////
    int status;
    /////////////////////////
    int               pid;
    char*             user_sp;
    char*             kernel_sp;
    /////////////////////////
    char*             signal_sp;
    /////////////////////////
    void              (*signal_handler[SIGNAL_MAX+1])();
    int               sigcount[SIGNAL_MAX + 1];
    void              (*curr_signal_handler)();
    int               signal_is_checking;
    thread_context_t  signal_saved_context;
    /////////////////////////
    // list_head_t       vma_list;
    struct vm_area_struct* vma_list;
    /////////////////////////

    /////////////////////////
    struct thread*    prev;
    struct thread*    next;
    /////////////////////////

    /////////////////////////
    char             curr_working_dir[MAX_PATH_NAME+1];
    struct file*     file_descriptors_table[MAX_FD+1];
    /////////////////////////
} thread_t;


extern thread_t *curr_thread;
extern thread_t *RQ_head;
//////////////////////////////////
extern thread_t *RQ_tail;
/////////////////////////////////
extern thread_t threads[MAX_THREAD_COUNT + 1];

/////////////////////////////////
void push_rq(thread_t* th);
thread_t* pop_rq();
/////////////////////////////////

void      schedule_timer(char *notuse);
void      init_thread_sched();
void      idle();
void      schedule();
void      kill_zombies();
void      thread_exit();
thread_t *thread_create(void *start, unsigned int filesize);
int       thread_exec(char *data, unsigned int filesize);

#endif /* _SCHED_H_ */