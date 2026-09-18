ENTRY(_start)
SECTIONS
{
    . = 0x80000;
    PROVIDE(_bootloader_start = .);
    .text.boot : {
        *(.text.boot)
        KEEP(*(.text.boot))
    }
    .text : {
        PROVIDE(_text_section_start = .);
        *(.text .text.* .gnu.linkonce.t*)
        PROVIDE(_text_section_end = .);
    }
    .rodata : { 
        PROVIDE(_rodata_section_start = .);
        *(.rodata .rodata.* .gnu.linkonce.r*)
        PROVIDE(_rodata_section_end = .); 
    }
    .data : {
        PROVIDE(_data_section_start = .);
        *(.data .data.* .gnu.linkonce.d*)
        PROVIDE(_data_section_end = .);
    }
    .bss : {
        PROVIDE(_bss_section_start = .);
        *(.bss .bss.*)
        *(COMMON)
        PROVIDE(_bss_section_end = .);
    }
    PROVIDE(_bootloader_end = .);

    _bss_size = (_bss_section_end - _bss_section_start);
    _bootloader_size = (_bootloader_end - _bootloader_start);
}
_relocation_address = 0x60000;


/* 
__bootloader_start = 0x60000;
__kernel_start = 0x80000;

SECTIONS
{
    . = __kernel_start;
    __begin = .;
    
    .text :   {
        KEEP(*(.text.boot)) 
        *(.text .text.*) 
    }
    .rodata :{ 
        *(.rodata .rodata.*) 
    }
    .data : { *(.data) }
    .bss :
    {
        . = ALIGN(8);

        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    }
    __end = .;
    
}

__bss_size = (__bss_end - __bss_start)>>3;
__relocation = 0x60000; */