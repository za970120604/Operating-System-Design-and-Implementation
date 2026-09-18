#include "bcm2837/rpi_mbox.h"
#include "mbox.h"

/* Aligned to 16-byte boundary while we have 28-bits for VC */
// volatile unsigned int  __attribute__((aligned(16))) pt[64];
volatile unsigned int  __attribute__((aligned(16))) pt[36];


int mbox_call( mbox_channel_type channel, unsigned int value )
{
    // Add channel to lower 4 bit
    value &= ~(0xF);
    value |= channel;
    while ( (*MBOX_STATUS & BCM_ARM_VC_MS_FULL) != 0 ) {}
    // Write to Register
    *MBOX_WRITE = value;
    while(1) {
        while ( *MBOX_STATUS & BCM_ARM_VC_MS_EMPTY ) {}
        // Read from Register
        if (value == *MBOX_READ)
            return pt[1] == MBOX_REQUEST_SUCCEED;
    }
    return 0;
}
/*
    In https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface:

    1. The buffer itself is 16-byte aligned as only the upper 28 bits of the address can be passed via the mailbox.
    2. All u64/u32/u16 values are in host CPU endian order.
    3. Tags should be processed in order except where an interface requires multiple tags for a single operation (like the frame buffer).
    4. Channels 8 and 9 are used.
            Channel 8: Request from ARM for response by VC
            Channel 9: Request from VC for response by ARM (none currently defined)
*/

/*
    課程網站-Mailbox

    Message:
        To pass messages by the mailbox, you need to prepare a message array. Then apply the following steps.

            1. Combine the message address (upper 28 bits) with channel number (lower 4 bits)

            2. Check if Mailbox 0 status register’s full flag is set.

            3. If not, then you can write to Mailbox 1 Read/Write register.

            4. Check if Mailbox 0 status register’s empty flag is set.

            5. If not, then you can read from Mailbox 0 Read/Write register.

            6. Check if the value is the same as you wrote in step 1.
*/

int mailbox_call(volatile unsigned int* mailbox) {
    unsigned char channel = 8;
    unsigned int message = ((unsigned int)((unsigned long)mailbox) & ~0xF) | (channel & 0xF);

    while (*MAILBOX_STATUS_REG & MAILBOX_FULL) {
        asm volatile("nop");
    }

    *MAILBOX_WRITE_REG = message;

    while (1) {
        while (*MAILBOX_STATUS_REG & MAILBOX_EMPTY) {
            asm volatile("nop");
        }

        if (*MAILBOX_READ_REG == message) {
            return (mailbox[1] == MAILBOX_RESPONSE); // Verify if the response is valid
        }
    }

    return 0;  // Should never reach here
}

/*  In https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface:

    Mailbox messages:
    The mailbox interface has 28 bits (MSB) available for the value and 4 bits (LSB) for the channel.
    Request message: 28 bits (MSB) buffer address
    Response message: 28 bits (MSB) buffer address
*/

/*
    In https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface:

    Buffer contents:
        u32: buffer size in bytes (including the header values, the end tag and padding)
        u32: buffer request/response code
            Request codes:
                0x00000000: process request
                All other values reserved
            Response codes:
                0x80000000: request successful
                0x80000001: error parsing request buffer (partial response)
                All other values reserved
        u8...: sequence of concatenated tags
        u32: 0x0 (end tag)
        u8...: padding

    Tag format:
        u32: tag identifier
        u32: value buffer size in bytes
        u32:
            Request codes:
                b31 clear: request
                b30-b0: reserved
            Response codes:
                b31 set: response
                b30-b0: value length in bytes
        u8...: value buffer
        u8...: padding to align the tag to 32 bits.
*/

/*
    Get board revision:
        Tag: 0x00010002
        Request:
            Length: 0
        Response:
            Length: 4 (4, x1 int)
            Value:
                u32: board revision
*/
unsigned int get_board_revision() {
    volatile unsigned int __attribute__((aligned(16))) mailbox[7];

    mailbox[0] = 7 * 4;  // Buffer size in bytes
    mailbox[1] = REQUEST_CODE;

    // Tags begin
    mailbox[2] = GET_BOARD_REVISION;  // Tag identifier
    mailbox[3] = 4;  // Maximum request and response buffer size
    mailbox[4] = TAG_REQUEST_CODE;
    mailbox[5] = 0;  // Value buffer
    // Tags end
    mailbox[6] = END_TAG;

    if (mailbox_call(mailbox)) {
        return mailbox[5];  // Return the board revision value
    } 
    else {
        return 0;  // Failure case
    }
}

/*
    Get ARM memory:
        Tag: 0x00010005
        Request:
            Length: 0
        Response:
            Length: 8 (4+4, x2 int)
            Value:
                u32: base address in bytes
                u32: size in bytes
*/
int get_arm_memory_info(unsigned int* base, unsigned int* size) {
    volatile unsigned int __attribute__((aligned(16))) mailbox[8];

    mailbox[0] = 8 * 4;  // Buffer size in bytes
    mailbox[1] = REQUEST_CODE;

    // Tags begin
    mailbox[2] = GET_ARM_MEMORY;  // Tag identifier
    mailbox[3] = 8;  // Maximum request and response buffer size
    mailbox[4] = TAG_REQUEST_CODE;
    mailbox[5] = 0;  // Base address buffer
    mailbox[6] = 0;  // Size buffer
    // Tags end
    mailbox[7] = END_TAG;

    if (mailbox_call(mailbox)) {
        *base = mailbox[5];  // Base address
        *size = mailbox[6];  // Size
        return 1;  // Success
    } 
    else {
        return 0;  // Failure
    }
}

