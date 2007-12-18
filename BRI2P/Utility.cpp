
#include "DSP.h"
#include "MsgBuf.h"

// Memory mapped registers definitions
//
#define DEFINE_MMAP_REG(name,value) \
	asm( "_" #name " .set " #value ); \
	asm( "    .def _" #name )

DEFINE_MMAP_REG( IMR,    0x0000 );
DEFINE_MMAP_REG( IFR,    0x0001 );
DEFINE_MMAP_REG( PMST,   0x001D );
DEFINE_MMAP_REG( XPC,    0x001E );
DEFINE_MMAP_REG( TIM,    0x0024 );
DEFINE_MMAP_REG( PRD,    0x0025 );
DEFINE_MMAP_REG( TCR,    0x0026 );
DEFINE_MMAP_REG( SWWSR,  0x0028 );
DEFINE_MMAP_REG( BSCR,   0x0029 );
DEFINE_MMAP_REG( SWCR,   0x002B );
DEFINE_MMAP_REG( HPIC,   0x002C );
DEFINE_MMAP_REG( CLKMD,  0x0058 );

// McBSP #0
DEFINE_MMAP_REG( DRR20,  0x0020 );
DEFINE_MMAP_REG( DRR10,  0x0021 );
DEFINE_MMAP_REG( DXR20,  0x0022 );
DEFINE_MMAP_REG( DXR10,  0x0023 );
DEFINE_MMAP_REG( SPSA0,  0x0038 );
DEFINE_MMAP_REG( SPSD0,  0x0039 );

// McBSP #1
DEFINE_MMAP_REG( DRR21,  0x0040 );
DEFINE_MMAP_REG( DRR11,  0x0041 );
DEFINE_MMAP_REG( DXR21,  0x0042 );
DEFINE_MMAP_REG( DXR11,  0x0043 );
DEFINE_MMAP_REG( SPSA1,  0x0048 );
DEFINE_MMAP_REG( SPSD1,  0x0049 );

// McBSP #2
DEFINE_MMAP_REG( DRR22,  0x0030 );
DEFINE_MMAP_REG( DRR12,  0x0031 );
DEFINE_MMAP_REG( DXR22,  0x0032 );
DEFINE_MMAP_REG( DXR12,  0x0033 );
DEFINE_MMAP_REG( SPSA2,  0x0034 );
DEFINE_MMAP_REG( SPSD2,  0x0035 );

unsigned short PLLMULT = 0xFFFF; // unknown PLL multiplier

///////////////////////////////////////////////////////////////////////////////
// Initialize DSP
//
void Init_DSP( void )
{
    // PMST Settings:                   Processor Mode Status Register
    //
    // xxxx xxxx x--- ----  IPTR        Interrupt vector pointer
    // ---- ---- -0-- ----  MP/~MC      Microprocessor/~microcomputer mode
    // ---- ---- --1- ----  OVLY        RAM overlay (DARAM0..3 -> Data&Prog space 0000-7fffh)
    // ---- ---- ---0 ----  AVIS        Address visibility mode
    // ---- ---- ---- 1---  DROM        Data ROM (DARAM4..7 -> Data space 8000-ffffh)
    // ---- ---- ---- -1--  CLKOFF      CLKOUT off
    // ---- ---- ---- --0-  SMUL        Saturation on multiplication
    // ---- ---- ---- ---0  SST         Saturation on store
    //
    extern void ISR_table( void );
    PMST = (unsigned short)ISR_table | 0x002C; // ISR table must be on 0x80 boundary!

    // BSCR Settings:                   Bus-Switching Control Register
    //
    // 1--- ---- ---- ----  ~CONSEC     Consecutive bank-switching
    // -11- ---- ---- ----  DIVFCT      CLKOUT divide factor
    // ---0 ---- ---- ----  IACKOFF     ~IACK signal output off
    // ---- 0000 0000 0---  reserved
    // ---- ---- ---- -1--  HBH         HPI HD0-HD7 bus holder
    // ---- ---- ---- --1-  BH          D0-D15 bus holder
    // ---- ---- ---- ---0  reserved
    //
    BSCR = 0xE006; // BH and HBH set to 1 (bus holders), and ~IACK disabled

    // SWWSR Settings:                  Software Wait-State Register
    //
    // 0--- ---- ---- ----  XPA         Extended address program control bit
    // -001 ---- ---- ----  I/O         I/O space
    // ---- 000- ---- ----  Data        Upper data space
    // ---- ---0 00-- ----  Data        Lower data space
    // ---- ---- --00 0---  Program     Upper program space
    // ---- ---- ---- -000  Program     Lower program space
    //
    SWWSR = 0x1000; // Set I/O Space access to be with 1 wait state IOSTR ~= 50ns

    // SWCR Settings:                   Software Wait-State Control Register
    //
    // 0000 0000 0000 000-  reserved
    // ---- ---- ---- ---0  SWSM        Software wait-state multiplier
    //
    SWCR = 0x0000; // wait-state base values are multiplied by 1

	IMR = 0x0000; // Mask (disable) all interrupts
	
    EnableIRQs (); // Enable all interrupts
    
    ///////////////////////////////////////////////////////////////////////////
    // Configure MCBSP#0 as BDR as input
    //
    volatile unsigned short* SPSA = &SPSA0;

    // SPCR1 settings:
    // 0--- ---- ---- ----  DLB     Digital loopback mode enable bit
    // -00- ---- ---- ----  RJUST       Receive sign-extension and justification mode
    // ---0 0--- ---- ----  CLKSTP      Clock stop mode
    // ---- -000 ---- ----  reserved
    // ---- ---- 0--- ----  DXENA       DX enabler
    // ---- ---- -0-- ----  ABIS        ABIS mode
    // ---- ---- --00 ----  RINTM       Receive interrupt mode
    // ---- ---- ---- 0---  RSYNCERR    Receive synchronization error
    // ---- ---- ---- -0--  RFULL       Receive shift register full
    // ---- ---- ---- --0-  RRDY        Receiver ready
    // ---- ---- ---- ---0  /RRST       Receiver reset. This resets and enables the receiver.
    //
    SPSA[ 0 ] = SPCR1;
    SPSA[ 1 ] = 0x0000;

    // PCR settings:
    // 00-- ---- ---- ----  reserved
    // --0- ---- ---- ----  XIOEN       Transmit general purpose I-O mode (only when /XRST = 0)
    // ---1 ---- ---- ----  RIOEN       Receive gneral purpose I-O mode (only when /RRST = 0)
    // ---- 0--- ---- ----  FSXM        Transmit frame-synchronization mode
    // ---- -0-- ---- ----  FSRM        Receive frame-synchronization mode
    // ---- --0- ---- ----  CLKXM       Transmitter clock mode
    // ---- ---0 ---- ----  CLKRM       Receiver clock mode
    // ---- ---- 0--- ----  reserved
    // ---- ---- -0-- ----  CLKS_STAT   CLSK pin status (when GPIO)
    // ---- ---- --0- ----  DX_STAT     DX pin status (when GPIO)
    // ---- ---- ---0 ----  DR_STAT     DR pin status (when GPIO)
    // ---- ---- ---- 0---  FSXP        Transmit frame-synhronization polarity
    // ---- ---- ---- -0--  FSRP        Receive frame-syncrhonization polarity
    // ---- ---- ---- --0-  CLKXP       Transmit clock polarity
    // ---- ---- ---- ---0  CLKRP       Receive clock polarity
    //
    SPSA[ 0 ] = PCR;
    SPSA[ 1 ] = 0x1000;
    }

void Init_PLL( unsigned int multiplier )
{
    // Change CLKMD to new value
    //
    // In order to switch from one PLL multiplier ratio to another multiplier ratio,
    // the following steps must be followed:
    // (see "Changing the PLL Multiplier Ratio" in SPRU131G
    // "TMS320C54x DSP Reference Set Volume 1: CPU and Peripherals")
    //
    // 1) Clear the PLLNDIV bit to 0, selecting DIV mode.
    //
    // 2) Poll the PLLSTATUS bit until a 0 is obtained, indicating that DIV mode is
    // enabled.
    //
    // 3) Modify CLKMD to set the PLLMUL, PLLDIV, and PLLNDIV bits to the
    // desired frequency multiplier.
    //
    // 4) Set the PLLCOUNT bits to the required lock-up time.
    //
    // Example 8.2: Switching Clock Mode From PLL × X Mode to PLL × 1 Mode
    //
    //    STM #0b, CLKMD ;switch to DIV mode
    // TstStatu: LDM CLKMD, A
    //    AND #01b, A ;poll STATUS bit
    //    BC TstStatu, ANEQ
    //    STM #0000001111101111b, CLKMD ;switch to PLL 
    //
    // CLKMD settings:                  Clock Mode Register
    //
    // 0110 ---- ---- ----  PLLMUL      PLL multiplier value
    // ---- 0--- ---- ----  PLLDIV      PLL divider
    // ---- -000 0000 1---  PLLCOUNT    PLL counter value
    // ---- ---- ---- -1--  PLLON/OFF   PLL On/Off mode bit
    // ---- ---- ---- --1-  PLLNDIV     0 = DIV mode, 1 = PLL mode
    // ---- ---- ---- ---1  PLLSTATUS   0 = DIV mode, 1 = PLL mode
    //
    CLKMD = 0;  // 1. switch to DIV mode
    do; while( ( CLKMD & 0x01 ) != 0 ); // 2. Poll status bit
    
    if ( multiplier == 0 )
    {
		PLLMULT = 0xFFFF;
    	return; // Set DIV mode
    	}
    	
    CLKMD = ( ( multiplier - 1 ) << 12 ) | 0x0007; // 3. & 4. Set PLL mode
    }

// Initialize Timer.
// Set clock period to be 1kHz according to CLKMD mode.
//
void Init_Timer( void )
{    
	PLLMULT = 1 + ( ( CLKMD & 0xF000 ) >> 12 ); // Set global variable
	
    // TCR settings:                    Timer Control Register
    //
    // **** ---- ---- ----  reserved    reserved
    // ---- 0--- ---- ----  SOFT        Timer stops 0 = immed, 1 = after reached 0
    // ---- -0-- ---- ----  FREE        0 = see SOFT bit, 1 = free running
    // ---- --00 00-- ----  PSC         Timer prescaler counter
    // ---- ---- --0- ----  TRB         Timer reload
    // ---- ---- ---1 ----  TSS         Timer stop status; 1 = stopped
    // ---- ---- ---- 0000  TDDR        Timer divide-down ratio
    //
	TCR = 0x0010; // Stop timer
	
    // CLKMD settings:                  Clock Mode Register
    //
    // mmmm ---- ---- ----  PLLMUL      PLL multiplier value
    // ---- 0--- ---- ----  PLLDIV      PLL divider
    // ---- -*** **** *---  PLLCOUNT    PLL counter value
    // ---- ---- ---- -1--  PLLON/OFF   PLL On/Off mode bit
    // ---- ---- ---- --1-  PLLNDIV     0 = DIV mode, 1 = PLL mode
    // ---- ---- ---- ---1  PLLSTATUS   0 = DIV mode, 1 = PLL mode
    //
	if ( ( CLKMD & 0x0807 ) == 0x0007 ) // PLL mode
	{
		// Set PRD := PLLMULT * 16384000 / 16 / f, where f = 1kHz
		// i.e. PRD := PLLMULT * 1024 = PLLMULT << 10
		//
    	PRD = PLLMULT << 10;
		TIM = 0; // reset counter
    	
	    // TCR settings:                    Timer Control Register
	    //
	    // **** ---- ---- ----  reserved    reserved
	    // ---- 0--- ---- ----  SOFT        Timer stops 0 = immed, 1 = after reached 0
	    // ---- -0-- ---- ----  FREE        0 = see SOFT bit, 1 = free running
	    // ---- --00 00-- ----  PSC         Timer prescaler counter
	    // ---- ---- --1- ----  TRB         Timer reload
	    // ---- ---- ---0 ----  TSS         Timer stop status; 1 = stopped
	    // ---- ---- ---- 1111  TDDR        Timer divide-down ratio
	    //
	    TCR = 0x002F; // Start timer
	    }
	else
	{
		// Unknown clock frequency; Keep timer stopped.
		}

	EnableIRQ( IRQMASK_TINT ); // Unmask TINT interrupt
    }

void *operator new( unsigned int )
{
	outBuf.Put( "ERROR: Operator new called.\n" );
	return NULL;
	}
