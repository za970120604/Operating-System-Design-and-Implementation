import os
import sys
import time

DEFAULT_FILENAME = "../kernel/kernel8.img"
BLOCK_SIZE = 1024
DEFAULT_UART = "/dev/ttyUSB0"

def get_file_size(filename):
    return os.stat(filename).st_size

def send_by_block(src_file, dst_file, file_size):
    dst_file.write(file_size.to_bytes(4, "big"))
    dst_file.flush()
    time.sleep(1)

    sent_bytes = 0
    while block := src_file.read(BLOCK_SIZE):
        dst_file.write(block)
        dst_file.flush()
        sent_bytes += len(block)
        print(f"Sent: {sent_bytes}/{file_size} bytes")
        time.sleep(0.5)

def send_by_byte(src_file, dst_file, file_size):
    raw_data = src_file.read()
    dst_file.write(file_size.to_bytes(4, "big"))
    dst_file.flush()
    time.sleep(1)

    sent_bytes = 0
    for i in range(file_size):
        dst_file.write(raw_data[i].to_bytes(1, "big"))
        dst_file.flush()
        sent_bytes += 1
        
        if sent_bytes % 1024 == 0:
            print(f"Sent: {sent_bytes}/{file_size} bytes")
        time.sleep(0.002)

def send_data(filename, uart_device, mode="block"):
    try:
        with open(uart_device, "wb", buffering=0) as tty, open(filename, 'rb') as file:
            file_size = get_file_size(filename)
            print(f"File size: {file_size} bytes")

            if mode == "block":
                send_by_block(file, tty, file_size)
            else:
                send_by_byte(file, tty, file_size)

            print("Kernel transmission completed.")

    except FileNotFoundError:
        print(f"Error: File '{filename}' not found.")
    except PermissionError:
        print(f"Error: Permission denied for '{uart_device}'. Try running with sudo.")

if __name__ == '__main__':
    uart_device = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_UART
    FILENAME = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_FILENAME
    send_data(FILENAME, uart_device, mode="block")
