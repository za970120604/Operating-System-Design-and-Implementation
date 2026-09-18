#include "header/uart.h"

void uart_init() {
    register unsigned int r;
    /* 
        課程網站-UART最上面

        Before using UART, you should configure GPIO pin to the corresponding mode.
        GPIO 14, 15 can be both used for mini UART and PL011 UART. 
        However, mini UART should set ALT5 and PL011 UART should set ALT0. You need to configure GPFSELn register to change alternate function.

        Next, you need to configure pull up/down register to disable GPIO pull up/down. 
        It’s because these GPIO pins use alternate functions, not basic input-output. Please refer to the description of GPPUD and GPPUDCLKn registers for a detailed setup.
    */
    /*
        P91.

        The function select registers are used to define the operation of the general-purpose I/O 
        pins. Each of the 54 GPIO pins  has at least two alternative functions as defined in section 
        16.2. The FSEL{n} field determines the functionality of the nth GPIO pin. 

        P92.

        Table 6-3 – GPIO Alternate function select register 1 
        17-15 FSEL15 FSEL15 - Function Select 15 R/W 0 
        14-12 FSEL14 FSEL14 - Function Select 14 R/W 0 
                    
    */
    // Configure GPIO14 and GPIO15 to alternate function 5 (UART TX/RX)
    r = *GPFSEL1;
    r &= ~((7 << 12) | (7 << 15)); // Clear GPIO14 and GPIO15 settings

    /*
        P102.

                Pull    ALT0    ALT1    ALT2        ALT3    ALT4    ALT5
        GPIO14  Low     TXD0    SD6     <reserved>                  TXD1 
        GPIO15  Low     RXD0    SD7     <reserved>                  RXD1 

    */
    /*
        P92.

        29-27   FSEL19  FSEL19 - Function Select 19 
                        000 = GPIO Pin 19 is an input 
                        001 = GPIO Pin 19 is an output 
                        100 = GPIO Pin 19 takes alternate function 0 
                        101 = GPIO Pin 19 takes alternate function 1 
                        110 = GPIO Pin 19 takes alternate function 2 
                        111 = GPIO Pin 19 takes alternate function 3 
                        011 = GPIO Pin 19 takes alternate function 4 
                        010 = GPIO Pin 19 takes alternate function 5
    */
    r |= (2 << 12) | (2 << 15);    // Set GPIO14 and GPIO15 to ALT5
    *GPFSEL1 = r;

    /*
        P101.

        The GPIO Pull-up/down Clock Registers control the actuation of internal pull-downs on 
        the respective GPIO pins. These registers must be used in conjunction with the GPPUD 
        register to effect GPIO Pull-up/down changes. 
        
        The following sequence of events is required: 

        1. Write to GPPUD to set the required control signal (i.e. Pull-up or Pull-Down or neither 
        to remove the current Pull-up/down) 

        2. Wait 150 cycles – this provides the required set-up time for the control signal 

        3. Write to GPPUDCLK0/1 to clock the control signal into the GPIO pads you wish to 
        modify – NOTE only the pads which receive a clock will be modified, all others will 
        retain their previous state. 

        4. Wait 150 cycles – this provides the required hold time for the control signal 

        5. Write to GPPUD to remove the control signal 

        6. Write to GPPUDCLK0/1 to remove the clock
    */
    /*
        P101.

        PUD :
            PUD - GPIO Pin Pull-up/down
            00 = Off – disable pull-up/down 
            01 = Enable Pull Down control 
            10 = Enable Pull Up control 
            11 = Reserved 
            *Use in conjunction with GPPUDCLK0/1/2
    */
    // Disable pull-up/down for GPIO14 and GPIO15
    *GPPUD = 0;
    r = 150;
    while (r--) { 
        asm volatile("nop"); 
    } // Wait for control signal setup

    /*
        GPIO control 54 pins
        GPPUDCLK0 controls 0-31 pins
        GPPUDCLK1 controls 32-53 pins
    */
    // Enable clock for GPIO14 and GPIO15
    *GPPUDCLK0 = (1 << 14) | (1 << 15);
    r = 150;
    while (r--) { 
        asm volatile("nop"); 
    } // Wait for clock signal setup

    // Remove control signals
    *GPPUD = 0;
    *GPPUDCLK0 = 0;

    // Small delay before initialization
    r = 300;
    while (r--) { 
        asm volatile("nop"); 
    }

    /*
        P9.

        If set the mini UART is enabled. The UART will 
        immediately start receiving data, especially if the 
        UART1_RX line is low. 
        If clear the mini UART is disabled. That also disables 
        any mini UART register access 
        
    */
    // Enables UART
    *AUX_ENABLES |= 1;

    /*
        P17.

        If this bit is set the mini UART receiver is enabled. 
        If this bit is clear the mini UART receiver is disabled 

    */
    // Disable UART receiver when setup
    *AUX_MU_CNTL_REG = 0;


    /*
        P12.

        If this bit is clear no receive interrupts are generated.

    */
    // Disable interrupts
    *AUX_MU_IER_REG = 0;

    /*
        P14.

        00: the UART works in 7-bit mode
        11: the UART works in 8-bit mode
        8 bits mode can use ASCII, Unicode, Char
    */
    // Set UART to 8-bit mode
    *AUX_MU_LCR_REG = 3;

    // Disable auto flow control
    *AUX_MU_MCR_REG = 0;

    // Set baud rate to 115200
    *AUX_MU_BAUD_REG = 270;

    /*
        P13.
        
        7:6 FIFO enables 
                Both bits always read as 1 as the FIFOs are always enabled

        2:1 READ: Interrupt ID bits 
            WRITE: FIFO clear bits 

                On read this register shows the interrupt ID bit 
                    00 : No interrupts 
                    01 : Transmit holding register empty 
                    10 : Receiver holds valid byte 
                    11 : <Not possible> 

                On write:  
                    Writing with bit 1 set will clear the receive FIFO 
                    Writing with bit 2 set will clear the transmit FIFO
    */
    // Clear FIFOs and disable FIFO buffer (low latency mode)
    *AUX_MU_IIR_REG = 0xC6;

    /*
        P17.

        1   Transmitter enable 
                If this bit is set the mini UART transmitter is enabled. 
                If this bit is clear the mini UART transmitter is 
                disabled 

        0   Receiver enable 
                If this bit is set the mini UART receiver is enabled. 
                If this bit is clear the mini UART receiver is disabled 
    */
    // Enable transmitter and receiver
    *AUX_MU_CNTL_REG = 3;
}


/*
    P15.
    5   Transmitter empty 
            This bit is set if the transmit FIFO can accept at least one byte. 
    0   Data ready 
            This bit is set if the receive FIFO holds at least 1 symbol.
*/

/*
    P11.

    The AUX_MU_IO_REG register is primary used to write data to and read data from the UART FIFOs. 

    7:0 Transmit data write, DLAB=0  
            Data written is put in the transmit FIFO (Provided it is not full) 
            (Only If bit 7 of the line control register (DLAB bit) is clear) 
    7:0 Receive data read, DLAB=0 
            Data read is taken from the receive FIFO (Provided it is not empty) 
            (Only If bit 7 of the line control register (DLAB bit) is clear) 
*/
void uart_send_char(unsigned int c) {
    // Wait until UART is ready to transmit
    while (!(*AUX_MU_LSR_REG & 0x20)) {
        asm volatile("nop");
    }
    // Write character to the transmit buffer
    *AUX_MU_IO_REG = c;
}


void uart_send_string(char *str) {
    while (*str) {
        if (*str == '\n') {
            uart_send_char('\r'); // Convert newline to carriage return
        }
        uart_send_char(*str);
        str++;
    }
}


char uart_recv_char() {
    // Wait until data is available in the receive buffer
    while (!(*AUX_MU_LSR_REG & 0x01)) {
        asm volatile("nop");
    }
    // Read character from UART
    char r = (char)(*AUX_MU_IO_REG);
    return (r == '\r') ? '\n' : r; // Convert carriage return to newline
}


void uart_recv_string(char *buf, int buf_size) {
    for (int i = 0; i < buf_size - 1; i++) {
        buf[i] = uart_recv_char();
        if (buf[i] == '\0') {
            return;
        }
    }
    buf[buf_size - 1] = '\0'; // Ensure null termination
}

unsigned char uart_recv_raw() {
    // Wait until data is available in the receive buffer
    while (!(*AUX_MU_LSR_REG & 0x01)) {
        asm volatile("nop");
    }
    // Read character from UART
    unsigned char r = (unsigned char)(*AUX_MU_IO_REG);
    return r;
}


void uart_binary_to_hex(unsigned int value) {
    char hex_str[11] = "0x00000000"; // Pre-fill with '0x' and zeros
    const char hex_digits[] = "0123456789ABCDEF";
    
    for (int i = 0; i < 8; i++) {
        hex_str[9 - i] = hex_digits[(value >> (i * 4)) & 0xF];
    }
    
    uart_send_string(hex_str);
}