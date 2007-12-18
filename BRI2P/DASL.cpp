
#include "DSP.h"
#include "MsgBuf.h"
#include "DASL.h"

///////////////////////////////////////////////////////////////////////////////

ioport unsigned int port0; // (R): D0=CINTn  (W): D0=CA0, D1=CEN 
ioport unsigned int port1; // (R): D0=COUT   (W): D0=CIN, D1=CCLK
ioport unsigned int port2; // (R): D0=M/Sn   (W): D0=M/Sn           (DASL #0)
ioport unsigned int port5; // (R): D0=M/Sn   (W): D0=M/Sn           (DASL #1)

///////////////////////////////////////////////////////////////////////////////

void DASL::Initialize( TDM_Sync mode )
{
	// Set DASL default mode: Slave, Power Down
	//
	control = 0x00;
	status = 0x00;
	Update ();

	// Set CPLD as as TDM Sync slave to DASL.
	// This is also default state after reset for CPLD.
	// This must be done first so we avoid collisions on the TDM bus.
	//
	if ( id == 0 )
		port2 = 0; // Set DASL #1 M/~S = 0
	else if ( id == 1 )
		port5 = 0; // Set DASL #1 M/~S = 0
	else
	{
		outBuf.Put( "Invalid DASL identifier." );
		
		// SEVERE ERROR: Block DSP and trigger watchdog on the host.
		//
		Freeze_CPU ();
		return;
		}

	// Configure DASL Master/Slave mode, and keep it still powered down.
	// At this point DASL should either give clock to TDM bus to CPLD,
	// or receive the clock from TDM bus from CPLD. In later case, because CPLD
	// is still configured as TDM slave, TDM bus will be in high impedance state.
	//
	if ( mode == MASTER )
		control &= ~0x80; 
	else
		control |= 0x80; // Slave, gives TDM clock to CPLD

	Update ();

	// Now, if DASL is configured as sync master on the line,
	// then CPLD should originate clock to DASL, so it should be also configured
	// as a master.
	//
	if ( IsMaster () )
	{
		if ( id == 0 )
			port2 = 1; // Set DASL #0 M/~S = 1
		else if ( id == 1 )
			port5 = 1; // Set DASL #1 M/~S = 1
		}
	}

///////////////////////////////////////////////////////////////////////////////
//

bool DASL::IsStatusChanged( void )
{
	// Note that CINT of DASLs are open drain outputs wired-OR to CINT input
	// on CPLD, thus fullfilling prerequisites for proper 'shared level-triggered 
	// interrupt' mode. CINT will state asserted until Update() is performed
	// on all DASL's.
	//
    return ( port0 & 1 ) == 0; // ~CINT == 1, i.e. it is not asserted
    }

///////////////////////////////////////////////////////////////////////////////
//

void DASL::Update( void )
{
	// Delayed time for ndelay(n) function is 10 + 2 * n CPU clocks 
	//
	// For 300 ns delay:
	//
	//   n = ( 300 ns * 16.384 MHz * PLLMULT - 10 ) / 2
	//   n = ( 5 * PLLMULT - 10 ) / 2;
	//
	// and backwards, based on previously calculated n:
	//
	//    82.74MHz : 10 + 2 *  7 = 24 CPU clocks * 12.23 ns = 293 ns
	//   163.48MHz : 10 + 2 * 20 = 50 CPU clocks *  6.12 ns = 306 ns
	//
	int Tdelay = ( 5 * PLLMULT - 10 ) / 2;
	
	port0 = 0;                     // Set CEN = 0, CA0 = 0
    port1 = 0;                     // Set CCLK = 0, CIN = 0

	ndelay( Tdelay );              // Delay >= 250ns

	int OUT = control;             // Load OUT with control word
    int curbit = ( OUT >> 7 ) & 1; // curbit = OUT.BIT[7]
    
	port1 = curbit;                // Set CCLK = 0
	port0 = id == 0 ? 0x02 : id == 1 ? 0x03 : 0x00; // Set CEN & CA0
	    
    int IN = 0;

    for ( int i = 0; i < 8; i++ )
    {
        ndelay( Tdelay - 4 );      // Delay >= 250ns;
                                   // -4 cycles to compesate read of port1 bellow
        IN <<= 1;                  // Shift left IN.BIT[]
        IN |= ( port1 & 1 );       // Read COUT into IN.BIT[0]

		port1 = curbit | 0x2;      // Set CCLK = 1

        ndelay( Tdelay );          // Delay >= 250ns

        OUT <<= 1;                 // Shift left OUT.BIT[]
		curbit = ( OUT >> 7 ) & 1; // curbit = OUT.BIT[7]
        port1 = curbit;            // Set CCLK = 0
        }

    ndelay( Tdelay );              // Delay >= 250ns
    
	port0 = 0;                     // Set CEN = 0, CA0 = 0
    
    status = IN;
    
	// tracef( "%c: LSD=%c (%c)", id, status, control );
    }
