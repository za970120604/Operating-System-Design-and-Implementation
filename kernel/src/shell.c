#include <stddef.h>
#include "shell.h"
#include "uart1.h"
#include "mbox.h"
#include "reboot.h"
#include "cpio.h"
#include "string.h"
#include "dtb.h"
#include "memory.h"
#include "timer.h"
#include "sched.h"

extern int   uart_recv_echo_flag;
extern void* CPIO_DEFAULT_START;
extern int SAFE_DEMO;

void cli_cmd_clear(char* buffer, int length) {
    for(int i=0; i<length; i++) {
        buffer[i] = '\0';
    }
}

void cli_cmd_read(char* buffer) {
    char c='\0';
    int idx = 0;
    while(1) {
        if ( idx >= 256 ) break;
        c = uart_async_getc();
        if ( c == '\n') {
            buffer[idx++] = '\0';
            break;
        }
        else {
            buffer[idx++] = c;
        }
    }
}

void cli_cmd_reboot() {
    uart_sendline("Rebooting....\n");
    volatile unsigned int* rst_addr = (unsigned int*)PM_RSTC;
    *rst_addr = PM_PASSWORD | 0x20;
    volatile unsigned int* wdg_addr = (unsigned int*)PM_WDOG;
    *wdg_addr = PM_PASSWORD | 5;
}

void cli_mailbox_info() {
    unsigned int board_revision = get_board_revision();
    if(board_revision != 0) {
        uart_sendline("My board model is: ");
        uart_binary_to_hex_long(board_revision); 
        uart_sendline("\r\n");
    }
    else {
        uart_sendline("Failed to get board model info.\r\n");
    }
    
    unsigned int base, size;
    if (get_arm_memory_info(&base, &size)) {
        uart_sendline("My ARM memory base address is: ");
        uart_binary_to_hex_long(base);
        uart_sendline("\r\n");

        uart_sendline("My ARM memory size is: ");
        uart_binary_to_hex_long(size);
        uart_sendline("\r\n");
    }
    else {
        uart_sendline("Failed to get ARM memory info.\r\n");
    }
}

void cli_setTimeout(char* arg) {
    if (arg) {
        char *msg = arg;
        char *sec = NULL;
        while (*msg) {
            if (*msg == ' ') {
                *msg = '\0';  
                sec = msg + 1; 
                break;
            }
            msg++;
        }
        
        if (sec == NULL || *sec == '\0') {
            uart_sendline("Usage: setTimeout <MESSAGE> <SECONDS>\n");
            return;
        }
        
        unsigned long sec_arg = utils_DecStr2Int(sec, utils_strlen(sec));
        msg = arg;
        add_timer(uart_puts, sec_arg, msg, 0);
    } 
    else {
        uart_sendline("Usage: setTimeout <MESSAGE> <SECONDS>\n");
    }
}

void cli_buddyinfo(char* arg) {
    if (arg) {
        unsigned long order_arg = utils_DecStr2Int(arg, utils_strlen(arg));
        print_order(order_arg, 1);
    } 
    else {
        uart_sendline("Usage: buddyinfo <target order>\n");
    }
}

void cli_exec(char* arg) {
    if (arg) {
        char* c_filedata;
        unsigned int c_filesize;
        int ret = cpio_find_program(arg,  &c_filesize, &c_filedata);
        if (ret > 0) {
            uart_recv_echo_flag = 0; 
            thread_exec(c_filedata, c_filesize);
        }
        else {
            uart_sendline("No program is found, so no thread_exec will happen.\r\n");
        }
    } 
    else {
        uart_sendline("Usage: exec <target program file>\n");
    }
}

void cli_display_help() {
    uart_sendline("help   : print this help menu\n");
    uart_sendline("hello  : print Hello World!\n");
    uart_sendline("info   : get the hardware's information\n");
    uart_sendline("reboot : reboot the device\n");
    uart_sendline("ls     : list all files in cpio archive folder\n");
    uart_sendline("cat    : show specific file content\n");
    uart_sendline("exec   : run the user program img if the provided img name exists in cpio archive\n");
    uart_sendline("async  : test asynchronous uart, it will asynchronously echos the user input\n");
    uart_sendline("setTimeout : set timeout duration and the message after timeout\n");
    uart_sendline("buddyinfo: list current target order freeframe list\n");
}

void cli_process_command(char *input_string) {
    char *cmd = input_string;
    char *arg = NULL;

    // Find the first space in the input
    while (*input_string) {
        if (*input_string == ' ') {
            *input_string = '\0';  // Replace space with null to separate command and argument
            arg = input_string + 1; // The argument starts after the space
            break;
        }
        input_string++;
    }

    if (utils_string_compare(cmd, "help") > 0) {
        cli_display_help();
    }
    else if (utils_string_compare(cmd, "hello") > 0) {
        uart_sendline("Hello World!\n");
    }
    else if (utils_string_compare(cmd, "info") > 0) {
        cli_mailbox_info();
    }
    else if (utils_string_compare(cmd, "reboot") > 0) {
        cli_cmd_reboot();
    }
    else if (utils_string_compare(cmd, "ls") > 0) {
        cpio_ls();
    }
    else if (utils_string_compare(cmd, "cat") > 0) {
        if (arg) {
            cpio_cat(arg);
        } 
        else {
            uart_sendline("Usage: cat <filename>\n");
        }
    }
    else if (utils_string_compare(cmd, "async") > 0) {
        // uart_sendline("Async Enter: ");
        // test_async_uart();
    }
    else if (utils_string_compare(cmd, "setTimeout") > 0) {
        cli_setTimeout(arg);
    }
    else if (utils_string_compare(cmd, "buddyinfo") > 0) {
        cli_buddyinfo(arg);
    }
    else if(utils_string_compare(cmd, "exec") > 0) {
        cli_exec(arg);
    }
    else {
        uart_sendline("Unknown command. Type 'help' for available commands.\n");
    }
}


void shell() {
    char cli_input_string[256];  
    while (1) {
        cli_cmd_clear(cli_input_string, 256);
        uart_sendline("# ");
        // cli_cmd_read(cli_input_string);
        // cli_process_command(cli_input_string);

        if (SAFE_DEMO) {
            uart_interrupt_disable();
        }

        cli_exec("vfs1.img");
    }
}


