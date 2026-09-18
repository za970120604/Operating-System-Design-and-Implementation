SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

ASMS = $(wildcard *.S)
ASM_OBJS = $(ASMS:.S=.o)


CFLAGS = -Wall -Wextra -Wpedantic -O0 -ffreestanding -nostdinc -nostdlib -nostartfiles -g

all: kernel8.img

%.o: %.S
	aarch64-linux-gnu-gcc $(CFLAGS) -c $< -o $@

%.o: %.c
	aarch64-linux-gnu-gcc $(CFLAGS) -c $< -o $@

kernel8.img: $(ASM_OBJS) $(OBJS)
	aarch64-linux-gnu-ld $(ASM_OBJS) $(OBJS) -T linker.ld -o kernel8.elf
	aarch64-linux-gnu-objcopy -O binary kernel8.elf kernel8.img

clean:
	rm kernel8.elf kernel8.img *.o >/dev/null 2>/dev/null || true

run_asm:
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -display none -d in_asm

run:
	qemu-system-aarch64 -M raspi3b -serial null -serial pty -display none -kernel bootloader.img -dtb bcm2710-rpi-3-b-plus.dtb -initrd initramfs.cpio

objdump:
	aarch64-linux-gnu-objdump -h kernel8.elf

disassembly:
	aarch64-linux-gnu-objdump -d kernel8.elf

debug:
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -display none -S -s

gdb:
	gdb-multiarch -ex "set architecture aarch64"
# End of snippet from Makefile