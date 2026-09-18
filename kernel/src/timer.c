#define STR(x) #x
#define XSTR(s) STR(s)
#include "timer.h"
#include "uart1.h"
#include "memory.h"
#include "utils.h"
#include "exception.h"

timer_event_t *timer_head;  // 頭節點
timer_event_t *timer_tail;  // 尾節點

void timer_list_init() {
    unsigned long long tmp;
    asm volatile("mrs %0, cntkctl_el1": "=r"(tmp));
    tmp |= 1;
    asm volatile("msr cntkctl_el1, %0":: "r"(tmp));

    // 創建頭尾節點
    timer_head = kmalloc(sizeof(timer_event_t));
    timer_tail = kmalloc(sizeof(timer_event_t));
    
    // 初始化頭尾節點
    timer_head->next = timer_tail;
    timer_head->prev = timer_head;
    timer_tail->next = timer_tail;
    timer_tail->prev = timer_head;
    
    // 設置一個無效的時間值
    timer_head->interrupt_time = 0;
    timer_tail->interrupt_time = 0xFFFFFFFFFFFFFFFF;  // 最大值
}

void core_timer_enable()
{
    __asm__ __volatile__(
        "mov x1, 1\n\t"
        "msr cntp_ctl_el0, x1\n\t" // enable

        "mov x2, 2\n\t"
        "ldr x1, =" XSTR(CORE0_TIMER_IRQ_CTRL) "\n\t"
        "str w2, [x1]\n\t" // unmask timer interrupt
    :::"x1","x2");
}

void core_timer_disable()
{
    __asm__ __volatile__(
        "mov x2, 0\n\t"
        "ldr x1, =" XSTR(CORE0_TIMER_IRQ_CTRL) "\n\t"
        "str w2, [x1]\n\t" // unmask timer interrupt
    :::"x1","x2");
}

// 檢查列表是否為空
int is_timer_list_empty() {
    return timer_head->next == timer_tail;
}

void timer_event_callback(timer_event_t *timer_event)
{
    ((void (*)(char *))timer_event->callback)(timer_event->args); // 調用回調函數
    
    // 從列表中移除事件
    timer_event->prev->next = timer_event->next;
    timer_event->next->prev = timer_event->prev;
    
    kfree(timer_event->args); // 釋放參數空間
    kfree(timer_event); // 釋放事件空間

    // 設置下一個事件的中斷時間
    if (!is_timer_list_empty()) {
        set_core_timer_interrupt_by_tick(timer_head->next->interrupt_time);
    } else {
        set_core_timer_interrupt(10000); // 禁用定時器中斷（設置一個很大的值）
    }
}

void core_timer_handler()
{
    disable_interrupt();
    if (is_timer_list_empty()) {
        set_core_timer_interrupt(10000); // 禁用定時器中斷（設置一個很大的值）
        enable_interrupt();
        return;
    }

    timer_event_callback(timer_head->next); // 執行回調並設置新的中斷
    enable_interrupt();
}

// 添加定時器
void add_timer(void *callback, unsigned long long timeout, char *args, int bytick)
{
    timer_event_t *new_timer_event = kmalloc(sizeof(timer_event_t));

    // 複製參數字符串
    unsigned long args_len = utils_strlen(args);
    new_timer_event->args = kmalloc(args_len + 1);
    utils_memcpy(new_timer_event->args, args, args_len + 1);

    // 計算中斷時間
    if (bytick == 0) {
        new_timer_event->interrupt_time = get_tick_plus_s(timeout);
    } 
    else {
        new_timer_event->interrupt_time = get_tick_plus_s(0) + timeout;
    }

    new_timer_event->callback = callback;

    disable_interrupt();
    
    // 找到適當的位置插入（按中斷時間排序）
    timer_event_t *current = timer_head;
    
    while (current->next != timer_tail) {
        if (current->next->interrupt_time > new_timer_event->interrupt_time)
            break;
        current = current->next;
    }
    
    // 插入新事件
    new_timer_event->next = current->next;
    new_timer_event->prev = current;
    current->next->prev = new_timer_event;
    current->next = new_timer_event;

    // 如果新事件是第一個，設置中斷
    if (timer_head->next == new_timer_event) {
        set_core_timer_interrupt_by_tick(new_timer_event->interrupt_time);
    }
    
    enable_interrupt();
}

// 獲取當前時間加上指定秒數的 tick
unsigned long long get_tick_plus_s(unsigned long long second)
{
    unsigned long long cntpct_el0 = 0;
    __asm__ __volatile__("mrs %0, cntpct_el0\n\t": "=r"(cntpct_el0));

    unsigned long long cntfrq_el0 = 0;
    __asm__ __volatile__("mrs %0, cntfrq_el0\n\t": "=r"(cntfrq_el0));

    return (cntpct_el0 + cntfrq_el0 * second);
}

// 設置定時器中斷為相對時間
void set_core_timer_interrupt(unsigned long long expired_time)
{
    __asm__ __volatile__(
        "mrs x1, cntfrq_el0\n\t"
        "mul x1, x1, %0\n\t"
        "msr cntp_tval_el0, x1\n\t"
        :: "r"(expired_time):"x1");
}

// 設置定時器中斷為絕對時間
void set_core_timer_interrupt_by_tick(unsigned long long tick)
{
    __asm__ __volatile__(
        "msr cntp_cval_el0, %0\n\t"
        :: "r"(tick));
}