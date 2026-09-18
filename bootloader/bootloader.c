#include "header/bootloader.h"

void main(){
    uart_init();

    unsigned char* kernel_addr = (unsigned char*)0x80000;
    unsigned int kernel_size = uart_recv_raw() << 24;
    kernel_size |= uart_recv_raw() << 16;
    kernel_size |= uart_recv_raw() << 8;
    kernel_size |= uart_recv_raw();


    for (int i = 0; i < kernel_size; i++){
        *(kernel_addr + i) = uart_recv_raw();
        if ((i+1)%1024 == 0) {
            uart_send_string("!\n");
        }
    }

    uart_send_string("Finished!\r\n");

    asm volatile(
        "mov x30, 0x80000;"
        "ret;"
    );

    return;
}
