#ifndef _DSP_H_INCLUDED
#define _DSP_H_INCLUDED

///////////////////////////////////////////////////////////////////////////////

static inline void *memcpy( void *to, const void *from, unsigned int n )
{
    register char *rto   = (char *) to;
    register char *rfrom = (char *) from;
    for ( register unsigned int rn = 0; rn < n; rn++ )
     	*rto++ = *rfrom++;
    return to;
	}

static inline void *memset( void *to, const char val, unsigned int n )
{
    register char *rto   = (char *) to;
    for ( register unsigned int rn = 0; rn < n; rn++ )
     	*rto++ = val;
    return to;
	}

///////////////////////////////////////////////////////////////////////////////
// Memory mapped registers declarations.
// See Utility.cpp for ASM definitions.
//
extern volatile unsigned short IMR;   // Interrupt Mask Register
extern volatile unsigned short IFR;   // Interrupt Flag Register
extern volatile unsigned short PMST;  // Processor Mode Status Register
extern volatile unsigned short XPC;   // Extended Program Counter
extern volatile unsigned short BSCR;  // Bus-Switching Control Register
extern volatile unsigned short SWWSR; // Software Wait-State Register
extern volatile unsigned short SWCR;  // Software Wait-State Control Register
extern volatile unsigned short HPIC;  // HPI Control Register
extern volatile unsigned short TCR;   // Timer Control Register
extern volatile unsigned short TIM;   // Timer Count Register
extern volatile unsigned short PRD;   // Timer Period Register
extern volatile unsigned short CLKMD; // Clock Mode Register

// McBSP #0:
extern volatile unsigned short DRR20; // McBSP Data Receive Register 2
extern volatile unsigned short DRR10; // McBSP Data Receive Register 1
extern volatile unsigned short DXR20; // McBSP Data Transmit Register 2
extern volatile unsigned short DXR10; // McBSP Data Transmit Register 1
extern volatile unsigned short SPSA0; // McBSP Subbank Address Register
extern volatile unsigned short SPSD0; // McBSP Subbank Data Register

// McBSP #1:
extern volatile unsigned short DRR21; // McBSP Data Receive Register 2
extern volatile unsigned short DRR11; // McBSP Data Receive Register 1
extern volatile unsigned short DXR21; // McBSP Data Transmit Register 2    
extern volatile unsigned short DXR11; // McBSP Data Transmit Register 1
extern volatile unsigned short SPSA1; // McBSP Subbank Address Register
extern volatile unsigned short SPSD1; // McBSP Subbank Data Register

 // McBSP #2:
extern volatile unsigned short DRR22; // McBSP Data Receive Register 2
extern volatile unsigned short DRR12; // McBSP Data Receive Register 1
extern volatile unsigned short DXR22; // McBSP Data Transmit Register 2    
extern volatile unsigned short DXR12; // McBSP Data Transmit Register 1
extern volatile unsigned short SPSA2; // McBSP Subbank Address Register
extern volatile unsigned short SPSD2; // McBSP Subbank Data Register

///////////////////////////////////////////////////////////////////////////////

// McBSP Control Registers and Subaddresses
//
enum
{
    SPCR1  = 0x0000, // McBSP serial port control register 1
    SPCR2  = 0x0001, // McBSP serial port control register 2
    RCR1   = 0x0002, // McBSP receive control register 1
    RCR2   = 0x0003, // McBSP receive control register 2
    XCR1   = 0x0004, // McBSP transmit control regsiter 1
    XCR2   = 0x0005, // McBSP transmit control register 2
    SRGR1  = 0x0006, // McBSP sample rate generator register 1
    SRGR2  = 0x0007, // McBSP sample rate generator register 2
    MCR1   = 0x0008, // McBSP multichannel register 1
    MCR2   = 0x0009, // McBSP multichannel register 2
    RCERA  = 0x000A, // McBSP receive channel enable register partition A
    RCERB  = 0x000B, // McBSP receive channel enable register partition B
    XCERA  = 0x000C, // McBSP transmit channel enable register partition A
    XCERB  = 0x000D, // McBSP transmit channel enable register partition B
    PCR    = 0x000E  // McBSP pin control register
    };
    
///////////////////////////////////////////////////////////////////////////////

extern volatile bool TINT_arrived;

// Delayed time: 10 + 2 * n CPU clocks
//
static inline void ndelay( unsigned short value )
{
    for ( unsigned short i = 0; i < value; i++ )
        asm( " NOP " );
    }

///////////////////////////////////////////////////////////////////////////////

enum IRQID
{
	IRQMASK_INT0      = 0x0001,  // DMA channel 5
	IRQMASK_INT1      = 0x0002,  // DMA channel 4
	IRQMASK_INT2      = 0x0004,  // McBSP #1 transmit
	IRQMASK_TINT      = 0x0008,  // McBSP #1 receive
	IRQMASK_RINT0     = 0x0010,  // HPI interrupt
	IRQMASK_XINT0     = 0x0020,  // External user interrupt #3
	IRQMASK_RINT2     = 0x0040,  // McBSP #2 transmit
	IRQMASK_XINT2     = 0x0080,  // McBSP #2 receive
	IRQMASK_INT3      = 0x0100,  // McBSP #0 transmit
	IRQMASK_HINT      = 0x0200,  // McBSP #0 receive
	IRQMASK_RINT1     = 0x0400,  // Timer interrupt
	IRQMASK_XINT1     = 0x0800,  // External user interrupt #2
	IRQMASK_DMAC4     = 0x1000,  // External user interrupt #1
	IRQMASK_DMAC5     = 0x2000   // External user interrupt #0
	};

// Enable interrupts, by mask
//
static inline void EnableIRQ( unsigned short mask )
{
    // IMR and IFR bit layouts:
    //
    // 00-- ---- ---- ----  reserved
    // --*- ---- ---- ----  DMAC5       DMA channel 5
    // ---* ---- ---- ----  DMAC4       DMA channel 4
    // ---- *--- ---- ----  XINT1       McBSP #1 transmit
    // ---- -*-- ---- ----  RINT1       McBSP #1 receive
    // ---- --*- ---- ----  HINT        HPI interrupt
    // ---- ---* ---- ----  INT3        External user interrupt #3
    // ---- ---- *--- ----  XINT2       McBSP #2 transmit
    // ---- ---- -*-- ----  RINT2       McBSP #2 receive
    // ---- ---- --*- ----  XINT0       McBSP #0 transmit
    // ---- ---- ---* ----  RINT0       McBSP #0 receive
    // ---- ---- ---- *---  TINT        Timer interrupt
    // ---- ---- ---- -*--  INT2        External user interrupt #2
    // ---- ---- ---- --*-  INT1        External user interrupt #1
    // ---- ---- ---- ---*  INT0        External user interrupt #0
    //
	IMR |= mask; // Enable interrupt
    IFR |= mask; // Clear any pending interrupts
    }

// Disable interrupts, by mask
//
static inline void DisableIRQ( unsigned short mask )
{
	IMR &= ~mask; // Disable interupt
    }
    
static inline void DisableIRQs( void ) 
{
	// 2 NOPs required in case 'ssbx intm' is proceeded by 'stlm st1'
	// 1 NOP required in case 'ssbx intm' is proceeded by 'pshm st1'
	asm( "        ;-------- Disable all interrupts" );
	asm( "        NOP" );
	asm( "        NOP" );
  	asm( "        SSBX INTM" );
	}
	
static inline void EnableIRQs( void ) 
{
	asm( "        ;-------- Enable all interrupts" );
	asm( "        XC 1, UNC" );
	asm( "        RSBX INTM" );
	}

///////////////////////////////////////////////////////////////////////////////

static inline void SetXF( void ) 
{
	asm( "        SSBX XF " );
	}
	
static inline void ResetXF( void ) 
{
	asm( "        RSBX XF " );
	}

///////////////////////////////////////////////////////////////////////////////

static inline void Freeze_CPU( void )
{
	for ( ;; ) {} // this will make CPU frozen, thus triggering c54load watchdog
	}

///////////////////////////////////////////////////////////////////////////////

extern unsigned short PLLMULT; // PLL multiplier value; base freq 16.384 MHz

///////////////////////////////////////////////////////////////////////////////

extern void Init_DSP( void );
extern void Init_PLL( unsigned int multiplier );
extern void Init_Timer( void );
extern void Init_DASL_McBSP( int id );
extern void Init_AIC_McBSP( int id );
extern void Reset_McBSP2( void );
extern void Watchdog_ACK( void );

extern int* vpoStack_ptr;
extern unsigned char* temp_msg;

extern bool Repeater_Mode;

///////////////////////////////////////////////////////////////////////////////

struct SystemParameters
{
	int ID;			         // DSP Identifier 
	int ChannelCount;        // Number of channels
	unsigned short TDM_MasterFlags; // TDM Master flags (bit 0 for channel 0 etc.)
	int PLLMULT           ;  // PLL multiplier
	};
	
extern const SystemParameters params;

///////////////////////////////////////////////////////////////////////////////

#endif // _DSP_H_INCLUDED
