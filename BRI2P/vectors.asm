;
;  Interrupt vectors
;

        .sect ".vectors"

        .def _ISR_table__Fv     ; C++ name for: void ISR_table( void )
        
        .ref _c_int00           ; C entry point
        
        .ref _TINT_handler
        .ref _HINT_handler
        
        .ref _INT0_handler
        .ref _INT1_handler
        .ref _INT2_handler
        .ref _INT3_handler
        
        .ref _XINT0_handler
        .ref _RINT0_handler
        .ref _XINT1_handler
        .ref _RINT1_handler
        .ref _XINT2_handler
        .ref _RINT2_handler
        
        .ref _DMA4_handler
        .ref _DMA5_handler
        
        .align  0x80            ; Must be aligned on page boundary
        
_ISR_table__Fv:

RESET:                          ; Reset (Hardware and software reset)
        BD _c_int00             ; branch to C entry point
        STM #200,SP             ; stack size of 200

NMI:                            ; Non maskable interrupt
		RETE
        NOP
        NOP
        NOP
        
sint17 .space 4*16              ; Software interrupt #17
sint18 .space 4*16              ; Software interrupt #18
sint19 .space 4*16              ; Software interrupt #19
sint20 .space 4*16              ; Software interrupt #20
sint21 .space 4*16              ; Software interrupt #21
sint22 .space 4*16              ; Software interrupt #22
sint23 .space 4*16              ; Software interrupt #23
sint24 .space 4*16              ; Software interrupt #24
sint25 .space 4*16              ; Software interrupt #25
sint26 .space 4*16              ; Software interrupt #26
sint27 .space 4*16              ; Software interrupt #27
sint28 .space 4*16              ; Software interrupt #28
sint29 .space 4*16              ; Software interrupt #29
sint30 .space 4*16              ; Software interrupt #30

int0:                           ; External user interrupt #0
		B _INT0_handler
        NOP
        NOP

int1:                           ; External user interrupt #1
		B _INT1_handler
        NOP
        NOP

int2:                           ; External user interrupt #2
		B _INT2_handler
        NOP
        NOP

tint:                           ; Timer interrupt
		B _TINT_handler
        NOP
        NOP

rint0:                          ; McBSP #0 receive interrupt
		B _RINT0_handler
        NOP
        NOP

xint0:                          ; McBSP #0 transmit interrupt
		B _XINT0_handler
        NOP
        NOP

rint2:                          ; McBSP #2 receive interrupt
		B _RINT2_handler
        NOP
        NOP

xint2:                          ; McBSP #2 transmit interrupt
		B _XINT2_handler
        NOP
        NOP

int3:                           ; External user interrupt #3
		B _INT3_handler
        NOP
        NOP

hint:                           ; HPI interrupt
		B _HINT_handler
        NOP
        NOP

rint1:                          ; McBSP #1 receive interrupt
		B _RINT1_handler
        NOP
        NOP
    
xint1:                          ; McBSP #1 transmit interrupt
		B _XINT1_handler
        NOP
        NOP

dmac4:                          ; DMA channel 4
		B _DMA4_handler
        NOP
        NOP

dmac5:                          ; DMA channel 5
		B _DMA5_handler
        NOP
        NOP

; reserved
        .space 4*16
        .space 4*16

        .end
