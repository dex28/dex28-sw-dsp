MEMORY
{
    PAGE 0: EPROG:      origin = 0x1000,        len = 0x1000

    PAGE 1: IDATA:      origin = 0x80,          len = 0xF80
}

SECTIONS
{
    .text:    {} > EPROG PAGE 0
}
