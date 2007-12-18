
MEMORY
{
    PAGE 0: EPROG:      origin = 0x8000,        len = 0x8000

    PAGE 1: CPUREGS:    origin = 0x0,           len = 0x005F
            USERREGS:   origin = 0x60,          len = 0x001c
            BIOSREGS:   origin = 0x7c,          len = 0x0004
            IDATA:      origin = 0x80,          len = 0x7F80
            EDATA:      origin = 0x8000,        len = 0x8000
}

SECTIONS
{
    .vectors: {} > EPROG PAGE 0
    .text:    {} > EPROG PAGE 0
    
    .sysregs: {} > BIOSREGS PAGE 1
    
    .sysparm: {} > IDATA PAGE 1
    .msgbuf:  {} > IDATA PAGE 1
    .bss:     {} > IDATA PAGE 1
    .MEM$obj: {} > IDATA PAGE 1
    .far:     {} > IDATA PAGE 1
    .sysheap: {} > IDATA PAGE 1
    .cio:     {} > IDATA PAGE 1
    .sysmem:  {} > IDATA PAGE 1
    .stack:   {} > IDATA PAGE 1
    .cinit:   {} > IDATA PAGE 1
    .pinit:   {} > IDATA PAGE 1
    .switch:  {} > IDATA PAGE 1
    .sysinit: {} > IDATA PAGE 1
    .const:   {} > IDATA PAGE 1
    .data:    {} > IDATA PAGE 1
    .d2_rev:  {} > IDATA PAGE 1, type=DSECT
    .vsb_rev: {} > IDATA PAGE 1, type=DSECT
}

