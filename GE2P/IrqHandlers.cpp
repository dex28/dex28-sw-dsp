
#include "DSP.h"
#include "MsgBuf.h"
#include "ELU28.h"

extern ELU28_D_Channel D0;
extern ELU28_D_Channel D1;

extern B_Channel B0;
extern B_Channel B1;

///////////////////////////////////////////////////////////////////////////////
/*
	Notes:
	
		McBSP #0  <->   DASL#0 (TP3406V)
		McBSP #1  <->   DASL#1 (TP3406V)
		McBSP #2  <->   not used
		
		INT0	DASL#0 D-channel
		INT1	DASL#1 D-channel
		INT2    not used
		INT3    DASL MicroWIRE Bus (polled; not used)
*/

ioport unsigned int port3; // R: D0=DR0    W: D0=DX0
ioport unsigned int port6; // R: D0=DR1    W: D0=DX1
#define D0_port     port3
#define D1_port     port6

///////////////////////////////////////////////////////////////////////////////

// Statistic counters:
volatile unsigned long counter_D0_irq = 0;
volatile unsigned long counter_D1_irq = 0;
volatile unsigned long counter_B0_irq = 0;
volatile unsigned long counter_B1_irq = 0;

///////////////////////////////////////////////////////////////////////////////

volatile bool TINT_arrived = false;

extern "C" interrupt void TINT_handler()
{
	TINT_arrived = true;

	D0.DecTimeoutTimer ();
	D1.DecTimeoutTimer ();

	extern Timer SysT1;
	SysT1.Decrement ();
    }

///////////////////////////////////////////////////////////////////////////////

extern "C" interrupt void HINT_handler()
{
    }

///////////////////////////////////////////////////////////////////////////////

extern "C" interrupt void RINT0_handler()
{
    counter_B0_irq++;
    EnableIRQs ();

	if ( Repeater_Mode )
	{
		DXR10 = 0;
		DXR11 = 0;
		
		if ( 1 ) // Mono
		{
			B0.io_buf.McBSP_IO( (int)DRR10 + (int)DRR11 );
			}
		else // Stereo
		{
			B0.io_buf.McBSP_IO( DRR10 );
			B0.io_buf.McBSP_IO( DRR11 );
			}
		
		return;
		}

	DXR10 = B0.io_buf.McBSP_IO( DRR10 );
    }

extern "C" interrupt void XINT0_handler()
{
	// Both DRR read and DXR write are done in RINT handler.
	// Rationale: RRDY comes with one bit clock delay after XRDY,
	// if receive and transmit clocks and frame sync pins are tied together.
	// RRDY will thus trigger RINT one McBSP clock later after XRDY indicatated 
	// that DXR was empty.
    }

///////////////////////////////////////////////////////////////////////////////

extern "C" interrupt void RINT1_handler()
{
	if ( Repeater_Mode )
		return;
		
    counter_B1_irq++;
    EnableIRQs ();

	DXR11 = B1.io_buf.McBSP_IO( DRR11 );
    }
    
extern "C" interrupt void XINT1_handler()
{
    }
    
///////////////////////////////////////////////////////////////////////////////

extern "C" interrupt void RINT2_handler()
{
    }
    
extern "C" interrupt void XINT2_handler()
{
    }
    
///////////////////////////////////////////////////////////////////////////////

extern "C" interrupt void INT0_handler()
{
    counter_D0_irq++;

    D0_port = D0.xmt_que.GetBit();
    D0.rcv_buf.PutBit( D0_port & 1 );
    }
    
///////////////////////////////////////////////////////////////////////////////

extern "C" interrupt void INT1_handler()
{
    counter_D1_irq++;

    D1_port = D1.xmt_que.GetBit();
    D1.rcv_buf.PutBit( D1_port & 1 );
    }

///////////////////////////////////////////////////////////////////////////////

extern "C" interrupt void INT2_handler()
{
    }
    
///////////////////////////////////////////////////////////////////////////////

extern "C" interrupt void INT3_handler()
{
    }

///////////////////////////////////////////////////////////////////////////////

extern "C" interrupt void DMA4_handler()
{
    }
    
///////////////////////////////////////////////////////////////////////////////

extern "C" interrupt void DMA5_handler()
{
    }

