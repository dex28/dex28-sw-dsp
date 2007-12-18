#include "DSP.h"
#include "MsgBuf.h"
#include "Cadence.h"
#include "xhfc.h"

#pragma DATA_SECTION(".sysparm")
const SystemParameters params = 
{
	0,      // DSP ID -- Set up by the coff loader
	2,      // Number of Channels on this DSP
	0x00,   // Bits for the TDM master or slave mode
	-1      // PLLMULT constant; -1 means do not change
};

///////////////////////////////////////////////////////////////////////////////
// Common temporary buffer and VPO stack

enum { VPOSTACK_SIZE = 3000 };

static int temp_buf[ VPOSTACK_SIZE ];

int* vpoStack_ptr = &temp_buf[ VPOSTACK_SIZE - 1 ];
unsigned char* temp_msg = (unsigned char*)temp_buf;

///////////////////////////////////////////////////////////////////////////////
// System Timer and CPU Idle Loop Counter (ILC)

Timer SysT1; // System Timer (every 1s)

unsigned long uptime  = 0; // Up-Time in seconds

enum ILC_STATE // Idle Loop Counter (ILC) State
{
	ILC_NOT_CALIBRATED = 0,
	ILC_CALIBRATING    = 1,
	ILC_CALIBRATED     = 2
};

enum // System Timer and Idle Loop Counter Calibration Constants:
{
	SYST1_PERIOD       = 1000, // 1 s normal period
	SYST1_CAL_DELAY    =  300, // 300 ms after startup begin calibration
	SYST1_CAL_DURATION =  200  // 200 ms calibration period
	//
	// Note: SYST1_PERIOD must be integer multiple of SYST1_CAL_DURATION
};

ILC_STATE ILC_State   = ILC_NOT_CALIBRATED;
unsigned long maxIdle = 0; // Maximum loop count while idle
unsigned long idle    = 0; // Current loop count while idle
//  CPU usage in promilles will be 1000 - 1000 * idle / maxIdle

///////////////////////////////////////////////////////////////////////////////

XHFC hfc;
//B_Channel B_ch[ 1 ];

///////////////////////////////////////////////////////////////////////////////

void Init_Interface( void )
{
	static char hello[] = "TDM[1,0]: Hello, World! (Build " __DATE__ " " __TIME__ ")\n";

	hello[ 6 ] = params.TDM_MasterFlags & 1 ? 'M' : 'S'; // channel #0
	hello[ 4 ] = params.TDM_MasterFlags & 2 ? 'M' : 'S'; // channel #1
	
	outBuf.Put( hello );

	hfc.Initialize( 0x00 ); // params.TDM_MasterFlags, 0 );
	hfc.port[ 0 ].Startup ();
	hfc.port[ 1 ].Startup ();
	}

int main( void )
{
    Init_DSP ();

	if ( params.PLLMULT > 0 )
    	Init_PLL( params.PLLMULT );

    Init_STDIO ();

    Init_Interface ();

    Init_Timer ();

	SysT1.SetOnceTimeout( SYST1_CAL_DELAY );
	ILC_State = ILC_NOT_CALIBRATED;

    idle = 0;
    
    for ( ;; )
    {
        idle++;

		// Handle input MsgBuf
		//
		if ( ! inBuf.IsEmpty () )
		{
			int len;
			MSGTYPE msg_type = inBuf.GetUnpack( len, temp_msg, 1024 );

			if ( msg_type == MSGTYPE_IO_B )
			{
				}
			else if ( msg_type == MSGTYPE_IO_D )
			{
				}
			else if ( msg_type == MSGTYPE_CTRL && len >= 2 && temp_msg[ 0 ] == 0x01 )
			{
				}
			}

		hfc.EH_Interrupt ();

		if ( TINT_arrived ) // Every 1ms
		{
			TINT_arrived = false; // Acknowledge event
    		hfc.EH_Timer ();
			}

        if ( SysT1.IsTimeoutArrived () ) // Initially after 100ms, then every 1s
        {
	        if ( ILC_State == ILC_CALIBRATED )
	        {
				static struct
				{
					unsigned short uptime_MSW; // up time in seconds; MSW
					unsigned short uptime_LSW; // LSW
					unsigned short cpu_usage;  // in promilles
					unsigned short ch0_state;
					unsigned short ch1_state;
					unsigned short pll_mult;   // PLL multiplier
					unsigned short p[ 6 ];     // trace information
					} watchdog;
	
				watchdog.uptime_MSW = uptime >> 16;
				watchdog.uptime_LSW = uptime & 0xFFFF;
				uptime++;
				
				watchdog.cpu_usage = idle >= maxIdle 
					? 0 
					: 1000 - 1000 * idle / maxIdle ; // CPU utilization in promilles

				watchdog.ch0_state = 0;
				watchdog.ch1_state = 0;
				
				watchdog.pll_mult = 1 + ( CLKMD >> 12 ); // Current PLL multiplier

				watchdog.p[ 0 ] = 0;
				watchdog.p[ 1 ] = 0;
				watchdog.p[ 2 ] = 0;
				watchdog.p[ 3 ] = 0;
				watchdog.p[ 4 ] = hfc.Get_R_IRQ_OVERVIEW ();
				watchdog.p[ 5 ] = hfc.Get_R_STATUS ();

				outBuf.Put( MSGTYPE_WATCHDOG, (const unsigned short*)&watchdog, sizeof( watchdog ) );
				}
	        else if ( ILC_State == ILC_NOT_CALIBRATED ) 
	        {
			    SysT1.SetOnceTimeout( SYST1_CAL_DURATION );
			    ILC_State = ILC_CALIBRATING;
	        	}
	        else // ILC_State == ILC_CALIBRATING
	        {
		        // Calibrate maximum idle loop count
		        //
	        	maxIdle = idle * ( SYST1_PERIOD / SYST1_CAL_DURATION );
	        	
	        	// Set normal state and proceed with startup
	        	//
			    uptime = 0;
			    SysT1.SetPeriodicTimeout( SYST1_PERIOD );
			    ILC_State = ILC_CALIBRATED;

			    // Enable communication channels
			    //
			    //hfc.port[ 0 ].Startup ();
			    //hfc.port[ 1 ].Startup ();
	        	}

            idle = 0; // reset idle-loop counter
            }
        }

    // return 0;

    }
