#ifndef MAILBOX_H
#define MAILBOX_H

#include "gpio.h"
#define MAILBOX_BASE    MMIO_BASE + 0xb880

#define MAILBOX_READ_REG       ((volatile unsigned int*)(MAILBOX_BASE+0x0))
#define MAILBOX_STATUS_REG     ((volatile unsigned int*)(MAILBOX_BASE+0x18))
#define MAILBOX_WRITE_REG      ((volatile unsigned int*)(MAILBOX_BASE+0x20))

#define MAILBOX_EMPTY   0x40000000
#define MAILBOX_FULL    0x80000000
#define MAILBOX_RESPONSE 0x80000000

#define GET_BOARD_REVISION  0x00010002
#define GET_ARM_MEMORY      0x00010005
#define REQUEST_CODE        0x00000000
#define REQUEST_SUCCEED     0x80000000
#define REQUEST_FAILED      0x80000001
#define TAG_REQUEST_CODE    0x00000000
#define END_TAG             0x00000000

int mailbox_call(volatile unsigned int* );
unsigned int get_board_revision();
int get_arm_memory_info(unsigned int* , unsigned int* );

#endif // MAILBOX_H
