#include "DSP.h"
#include "MsgBuf.h"
#include "ELU28.h"

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
// True when DASL1 is Master and DASL0 is Slave

bool Repeater_Mode = false;

///////////////////////////////////////////////////////////////////////////////
// Channel 0

B_Channel B0( 0 );
ELU28_D_Channel D0( 0, &B0 );

///////////////////////////////////////////////////////////////////////////////
// Channel 1

B_Channel B1( 1 );
ELU28_D_Channel D1( 1, &B1 );

///////////////////////////////////////////////////////////////////////////////
// LED

ioport unsigned int port4;
ioport unsigned int port7;

#define LED0_port   port4 // W: D1=LEDY0, D0=LEDG0
#define LED1_port   port7 // W: D1=LEDY1, D0=LEDG1

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
	SYST1_CAL_DELAY    =  200, // 200 ms after startup begin calibration
	SYST1_CAL_DURATION =  100  // 100 ms calibration period
	//
	// Note: SYST1_PERIOD must be integer multiple of SYST1_CAL_DURATION
};

ILC_STATE ILC_State   = ILC_NOT_CALIBRATED;
unsigned long maxIdle = 0; // Maximum loop count while idle
unsigned long idle    = 0; // Current loop count while idle
//  CPU usage in promilles will be 1000 - 1000 * idle / maxIdle

///////////////////////////////////////////////////////////////////////////////

void Init_DASLs( void )
{
	static char hello[] = "TDM[1,0]: Hello, World! (Build " __DATE__ " " __TIME__ ")\n";

	hello[ 6 ] = params.TDM_MasterFlags & 1 ? 'M' : 'S'; // channel #0
	hello[ 4 ] = params.TDM_MasterFlags & 2 ? 'M' : 'S'; // channel #1
	
	outBuf.Put( hello );

	D0.dasl.Initialize( params.TDM_MasterFlags & 1 ? DASL::MASTER : DASL::SLAVE );
	D1.dasl.Initialize( params.TDM_MasterFlags & 2 ? DASL::MASTER : DASL::SLAVE );
	
	Repeater_Mode = D0.dasl.IsSlave () && D1.dasl.IsMaster();
	}

int main( void )
{
    Init_DSP ();

	if ( params.PLLMULT > 0 )    
    	Init_PLL( params.PLLMULT );

    Init_STDIO ();

    Init_DASLs ();

    Init_Timer ();

	SysT1.SetOnceTimeout( SYST1_CAL_DELAY );
	ILC_State = ILC_NOT_CALIBRATED;

    idle = 0;
    
    for ( ;; )
    {
        idle++;

        // Handle D channels
        //
        D0.RcvBuf_EH ();
        D1.RcvBuf_EH ();

		// Handle B channels
		//
		B0.DataReady_EH ();
		B1.DataReady_EH ();

		// Handle input MsgBuf
		//
		if ( ! inBuf.IsEmpty () )
		{
			int len;
			MSGTYPE msg_type = inBuf.GetUnpack( len, temp_msg, 1024 );

			if ( msg_type == MSGTYPE_IO_B )
			{
	            B_Channel* Bp = NULL;
	        	switch( temp_msg[ 0 ] ) // channel
	        	{
	        		case 0: Bp = &B0; break;
	        		case 1: Bp = &B1; break;
	        		}

	        	if ( Bp )
	        	{
	        		unsigned long L_ts = Bp->GetLocalTS ();
					Bp->jit_buf.Put( L_ts, temp_msg + 1, len - 1 ); // RTP Packet
	        		}
				}
			else if ( msg_type == MSGTYPE_IO_D )
			{
	            ELU28_D_Channel* Dp = NULL;
	        	switch( temp_msg[ 0 ] ) // channel
	        	{
	        		case 0: Dp = &D0; break;
	        		case 1: Dp = &D1; break;
	        		}

				if ( Dp )
				{
		        	if ( temp_msg[ 2 ] == FNC_RESET_REMOTE_DTSS ) // Broadcast Reset
		        	{
		        		// Keep temp_msg[ 0 ] == channel ID
						temp_msg[ 1 ] = 0x20;
						temp_msg[ 2 ] = FNC_EQUTESTREQ;
						temp_msg[ 3 ] = FNC_TEST_RESET;
						temp_msg[ 4 ] = 0x00;
						temp_msg[ 5 ] = 0x00;
						temp_msg[ 6 ] = 0x00;
						temp_msg[ 7 ] = 0x00;
	                    outBuf.PutPack( MSGTYPE_IO_D, temp_msg, 8 );
		        		}
		        	else if ( temp_msg[ 2 ] == FNC_KILL_REMOTE_IDODS ) // Broadcast Kill
		        	{
		        		// Keep temp_msg[ 0 ] == channel ID
						temp_msg[ 1 ] = 0x00;
						temp_msg[ 2 ] = FNC_KILL_REMOTE_IDODS;
	                    outBuf.PutPack( MSGTYPE_IO_D, temp_msg, 3 );
		        		}
		        	else if ( temp_msg[ 2 ] == FNC_DASL_LOOP_SYNC ) // Set Loop State 
		        	{
						if ( Dp->dasl.IsSlave () )
							Dp->DelayedPowerUp ();
		        		}
		        	else if ( temp_msg[ 2 ] == FNC_XMIT_FLOW_CONTROL ) // Ignore this
		        	{
		        		}
		        	else
		        	{
		        		if ( Dp->dasl.IsSlave () && Dp->delayed_power_up )
		        			; // do not pass messages to PBX during delayed power up
		        		else
							Dp->xmt_que.PutPDU( temp_msg + 1, len - 1 );

						if ( Dp->dasl.IsMaster () )
				     		Dp->ParsePDU_FromPBX( temp_msg + 1, len - 1 );
				     	else
				     		Dp->ParsePDU_FromDTS( temp_msg + 1, len - 1 );
	    				}
			     	}
				}
			else if ( msg_type == MSGTYPE_CTRL && len >= 2 && temp_msg[ 0 ] == 0x01 )
			{
	            B_Channel* Bp = NULL;
	        	switch( temp_msg[ 1 ] ) // channel
	        	{
	        		case 0: Bp = &B0; break;
	        		case 1: Bp = &B1; break;
	        		}

	        	if ( Bp )
	        	{
					Bp->ChangeParameters( temp_msg + 2, len - 2 );
					}
				}
			}

		if ( TINT_arrived ) // Every 1ms
		{
			TINT_arrived = false; // Acknowledge event

	        // Handle D channel timeout events
	        //
	        D0.Timed_EH ();
	        D1.Timed_EH ();

	        // Handle D channel DASL events
	        //
	        if ( DASL::IsStatusChanged() && ILC_State == ILC_CALIBRATED )
	        {
	        	D0.DASL_EH ();
	        	D1.DASL_EH ();
	        	}

			// Green LED cadence state update
			//
			D0.GreenLED.Increment ();
			D0.YellowLED.Increment ();
			LED0_port = ( D0.YellowLED.GetState () << 1 ) | D0.GreenLED.GetState ();
			
			// Yellow LED cadence state update
			//
			D1.GreenLED.Increment ();
			D1.YellowLED.Increment ();
			LED1_port = ( D1.YellowLED.GetState () << 1 ) | D1.GreenLED.GetState ();
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

				watchdog.ch0_state = D0.GetVerbState ();
				watchdog.ch1_state = D1.GetVerbState ();
				
				watchdog.pll_mult = 1 + ( CLKMD >> 12 ); // Current PLL multiplier
/*
				watchdog.p[ 0 ] = D0.GetFaultCounter ();
				watchdog.p[ 1 ] = D0.GetTimeoutCounter ();
				watchdog.p[ 2 ] = D0.GetDroppedCounter ();
				watchdog.p[ 3 ] = D0.xmt_que.IsEnhancedProtocol ();
*/				
/*
				watchdog.p[ 0 ] = B0.tx_depleted_count;
				watchdog.p[ 1 ] = B0.packets_count;
				watchdog.p[ 2 ] = B0.pred_adjusts_count;
				watchdog.p[ 3 ] = B0.JLen;
				watchdog.p[ 4 ] = B0.deltaRL_var;
				watchdog.p[ 5 ] = B0.ec_obj.status;
*/
				if ( Repeater_Mode )
				{
					watchdog.p[ 0 ] = D0.dasl.GetStatus (); // B0.tx_depleted_count;
					watchdog.p[ 1 ] = D1.dasl.GetStatus (); // B0.packets_count;
					watchdog.p[ 2 ] = B0.vrvad_avg;
					watchdog.p[ 3 ] = B0.param.VRVAD_Threshold;
					watchdog.p[ 4 ] = B0.vrvad_hangover;
					watchdog.p[ 5 ] = B0.param.VRVAD_HangoverTime;
					}
				else
				{
					watchdog.p[ 0 ] = D0.dasl.GetStatus (); // B0.tx_depleted_count;
					watchdog.p[ 1 ] = D1.dasl.GetStatus (); // B0.packets_count;
					watchdog.p[ 2 ] = B0.jit_buf.packets_count;
					watchdog.p[ 3 ] = B0.pred_adjusts_count;
					watchdog.p[ 4 ] = B0.jit_buf.JLen >> 4;
					watchdog.p[ 5 ] = B0.playRemoteSeqNo;
					}

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
			    D0.Initialize ();
			    D1.Initialize ();
	        	}

            idle = 0; // reset idle-loop counter
            }
        }

    // return 0;

    }
