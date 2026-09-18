#include "bcm2837/rpi_gpio.h"
#include "bcm2837/rpi_uart1.h"
#include "bcm2837/rpi_irq.h"
#include "uart1.h"
#include "exception.h"
#include "string.h"

//implement first in first out buffer with a read index and a write index
char uart_tx_buffer[VSPRINT_MAX_BUF_SIZE]={};
unsigned int uart_tx_buffer_widx = 0;  //write index
unsigned int uart_tx_buffer_ridx = 0;  //read index
char uart_rx_buffer[VSPRINT_MAX_BUF_SIZE]={};
unsigned int uart_rx_buffer_widx = 0;
unsigned int uart_rx_buffer_ridx = 0;

int uart_recv_echo_flag = 1;
extern int SAFE_DEMO;

// void uart_init()
// {
//     register unsigned int r;

//     /* initialize UART */
//     *AUX_ENABLES     |= 1;       // enable UART1
//     *AUX_MU_CNTL_REG  = 0;       // disable TX/RX

//     /* configure UART */
//     *AUX_MU_IER_REG   = 0;       // disable interrupt
//     *AUX_MU_LCR_REG   = 3;       // 8 bit data size
//     *AUX_MU_MCR_REG   = 0;       // disable flow control
//     *AUX_MU_BAUD_REG  = 270;     // 115200 baud rate
//     *AUX_MU_IIR_REG   = 0xC6;    // disable FIFO

//     /* map UART1 to GPIO pins */
//     r = *GPFSEL1;
//     r &= ~(7<<12);               // clean gpio14
//     r |= 2<<12;                  // set gpio14 to alt5
//     r &= ~(7<<15);               // clean gpio15
//     r |= 2<<15;                  // set gpio15 to alt5
//     *GPFSEL1 = r;

//     /* enable pin 14, 15 - ref: Page 101 */
//     *GPPUD = 0;
//     r=150; while(r--) { asm volatile("nop"); }
//     *GPPUDCLK0 = (1<<14)|(1<<15);
//     r=150; while(r--) { asm volatile("nop"); }
//     *GPPUDCLK0 = 0;

//     *AUX_MU_CNTL_REG = 3;      // enable TX/RX
// }

void uart_init() {
    register unsigned int r;
    //Since We've set alt5, we want to disable basic input/output
    //To achieve this, we need diable pull-up and pull-dwon
    *GPPUD = 0;   //  P101 top. 00- = off - disable pull-up/down 
    //Wait 150 cycles
    //this provides the required set-up time for the control signal 
    r=150; 
    while(r--) { 
        asm volatile("nop"); 
    }
    // GPIO control 54 pins
    // GPPUDCLK0 controls 0-31 pins
    // GPPUDCLK1 controls 32-53 pins
    // set 14,15 bits = 1 which means we will modify these two bits
    // trigger: set pins to 1 and wait for one clock
    *GPPUDCLK0 = (1<<14)|(1<<15);
    r=150; 
    while(r--) {
        asm volatile("nop"); 
    }
    *GPPUD = 0;           // remove control signal
    *GPPUDCLK0 = 0;        // flush GPIO setup


    r=500; while(r--) { asm volatile("nop"); }

    /* initialize UART */
    *AUX_ENABLES |=1;       
    //P.9: If set the mini UART is enabled. The UART will
    //immediately start receiving data, especially if the
    //UART1_RX line is low.
    //If clear the mini UART is disabled. That also disables
    //any mini UART register access 
    *AUX_MU_CNTL_REG = 0;
   //P.17 If this bit is set the mini UART receiver is enabled.
   //If this bit is clear the mini UART receiver is disabled
   //Prevent data exchange in initialization process
    *AUX_MU_IER_REG = 0;
   //Set AUX_MU_IER_REG to 0. 
   //Disable interrupt because currently you don’t need interrupt.
    *AUX_MU_LCR_REG = 3;       
   //P.14: 00 : the UART works in 7-bit mode
   //11(3) : the UART works in 8-bit mode
   //Cause 8 bits can use in ASCII, Unicode, Char
    *AUX_MU_MCR_REG = 0;
   //Don’t need auto flow control.
   //AUX_MU_MCR is for basic serial communication. Don't be too smart
    *AUX_MU_BAUD_REG = 270;
   //set BAUD rate to 115200(transmit speed)
   //so we need set AUX_MU_BAUD to 270 to meet the goal
    *AUX_MU_IIR_REG = 0xc6;
   // bit 6 bit 7 No FIFO. Sacrifice reliability(buffer) to get low latency    // 0xc6 = 11000110
   // Writing with bit 1 set will clear the receive FIFO
   // Writing with bit 2 set will clear the transmit FIFO
   // Both bits always read as 1 as the FIFOs are always enabled  
    /* map UART1 to GPIO pins */
    *AUX_MU_CNTL_REG = 3; // enable Transmitter,Receiver
    r=*GPFSEL1;
    r&=~((7<<12)|(7<<15)); // gpio14, gpio15 clear to 0
    r|=(2<<12)|(2<<15);    // set gpio14 and 15 to 010/010 which is alt5
    *GPFSEL1 = r;          // from here activate Trasmitter&Receiver
}

char uart_recv() {
    char r;
    while(!(*AUX_MU_LSR_REG & 0x01)){};
    r = (char)(*AUX_MU_IO_REG);
    if(uart_recv_echo_flag){
        uart_send(r);
        if(r =='\r') {uart_send('\r');uart_send('\n');}
    }
    return r=='\r'?'\n':r;
}

void uart_send(char c) {
    while(!(*AUX_MU_LSR_REG & 0x20)){};
    *AUX_MU_IO_REG = c;
}

void uart_2hex(unsigned int d) {
    unsigned int n;
    int c;
    for(c=28;c>=0;c-=4) {
        n=(d>>c)&0xF;
        n+=n>9?0x37:0x30;
        uart_send(n);
    }
}

int uart_sendline(char* fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    char buf[VSPRINT_MAX_BUF_SIZE];

    char *str = (char*)buf;
    int count = vsprintf(str,fmt,args);

    while(*str) {
        if(*str=='\n')
            uart_send('\r');
        uart_send(*str++);
    }
    __builtin_va_end(args);
    return count;
}


// uart_async_getc read from buffer
// uart_r_irq_handler write to buffer then output
char uart_async_getc() {
    *AUX_MU_IER_REG |=1; // enable read interrupt
    // do while if buffer empty
    while (uart_rx_buffer_ridx == uart_rx_buffer_widx) *AUX_MU_IER_REG |=1; // enable read interrupt
    disable_interrupt();
    char r = uart_rx_buffer[uart_rx_buffer_ridx++];
    if (uart_rx_buffer_ridx >= VSPRINT_MAX_BUF_SIZE) uart_rx_buffer_ridx = 0;
    enable_interrupt();
    return r;
}


// uart_async_putc writes to buffer
// uart_w_irq_handler read from buffer then output
void uart_async_putc(char c) {
    // if buffer full, wait for uart_w_irq_handler
    while( (uart_tx_buffer_widx + 1) % VSPRINT_MAX_BUF_SIZE == uart_tx_buffer_ridx )  *AUX_MU_IER_REG |=2;  // enable write interrupt
    disable_interrupt();
    uart_tx_buffer[uart_tx_buffer_widx++] = c;
    if(uart_tx_buffer_widx >= VSPRINT_MAX_BUF_SIZE) uart_tx_buffer_widx=0;  // cycle pointer
    enable_interrupt();
    *AUX_MU_IER_REG |=2;  // enable write interrupt
}

int  uart_puts(char* fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    char buf[VSPRINT_MAX_BUF_SIZE];

    char *str = (char*)buf;
    int count = vsprintf(str,fmt,args);

    while(*str) {
        if(*str=='\n')
            uart_async_putc('\r');
        uart_async_putc(*str++);
    }
    __builtin_va_end(args);
    return count;
}

// AUX_MU_IER_REG -> BCM2837-ARM-Peripherals.pdf - Pg.12
void uart_interrupt_enable(){
    *AUX_MU_IER_REG |=1;  // enable read interrupt
    *AUX_MU_IER_REG |=2;  // enable write interrupt
    *ENABLE_IRQS_1 |= (1 << 29);    // Pg.112
}

void uart_interrupt_disable(){
    *AUX_MU_IER_REG &= ~(1);  // disable read interrupt
    *AUX_MU_IER_REG &= ~(2);  // disable write interrupt
}


void uart_r_irq_handler(){
    if((uart_rx_buffer_widx + 1) % VSPRINT_MAX_BUF_SIZE == uart_rx_buffer_ridx)
    {
        *AUX_MU_IER_REG &= ~(1);  // disable read interrupt
        return;
    }
    uart_rx_buffer[uart_rx_buffer_widx++] = uart_recv();
    if(uart_rx_buffer_widx>=VSPRINT_MAX_BUF_SIZE) uart_rx_buffer_widx=0;
    *AUX_MU_IER_REG |=1;
}

void uart_w_irq_handler(){
    if(uart_tx_buffer_ridx == uart_tx_buffer_widx)
    {
        *AUX_MU_IER_REG &= ~(2);  // disable write interrupt
        return;  // buffer empty
    }
    uart_send(uart_tx_buffer[uart_tx_buffer_ridx++]);
    if(uart_tx_buffer_ridx>=VSPRINT_MAX_BUF_SIZE) uart_tx_buffer_ridx=0;
    *AUX_MU_IER_REG |=2;  // enable write interrupt
}

void uart_binary_to_hex_long(unsigned long value) {
    char hex_str[19];  // "0x" (2) + 16 hex digits + '\0' (1) = 19
    const char hex_digits[] = "0123456789ABCDEF";

    // Add the "0x" prefix
    hex_str[0] = '0';
    hex_str[1] = 'x';

    // Convert the unsigned long to hexadecimal and store it in the string
    for (int i = 0; i < 16; i++) {
        hex_str[2 + i] = hex_digits[(value >> ((15 - i) * 4)) & 0xF];
    }

    hex_str[18] = '\0';  // Null-terminate the string

    // Send the string via UART
    uart_sendline(hex_str);
}

