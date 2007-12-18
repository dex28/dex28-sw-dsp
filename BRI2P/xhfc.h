#ifndef _XHFC_H_INCLUDED
#define _XHFC_H_INCLUDED

#include "DSP.h"
#include "MsgBuf.h"
#include "B_Channel.h"

extern "C" 
{
    #include "mISDN/xhfc24succ.h"
}

static ioport unsigned int port0;
	
//////////////////////////////////////////////////////////////////////////////////////
// XHFC Controller Low Level I/O
//
class XHFC_HW
{
	enum { tCycle = 6 };

	// Requirement: tCycle should be at least 3.5 * tSYS
	// fSYS = 24.576 MHz --> tSYS = 40.7 ns
	// tCycle > 3.5 * 40.7 ns = 143 ns
	//
	// Measured: 
	// ndelay( 6 ) @ DSP PLL 10 x 16.384 MHz : Real tCycle = 147 ns 
	// ndelay( 6 ) @ DSP PLL  7 x 16.384 MHz : Real tCycle = 210 ns 
	// ndelay( 6 ) @ DSP PLL  5 x 16.384 MHz : Real tCycle = 293 ns 
	// 
public:

    bool IsIrqAsserted( void ) const
    {
        // BDR0 is connected to INT#

	    // McBSP#0 PCR settings:
	    // ---- ---- ---0 ----  DR_STAT     DR pin status (when GPIO)
    	//
    	volatile unsigned short* SPSA = &SPSA0;
    	SPSA[ 0 ] = PCR;
    	return !( SPSA[ 1 ] & 0x0010 );
        }

    int Read( int addr )
    {
    	ndelay( tCycle );
    	
    	SetXF (); // A0 = 1 (addr register access)
    	asm( " NOP " ); asm( " NOP " ); asm( " NOP " ); // tAS >= 20 ns
    	port0 = addr;
    	
    	ndelay( tCycle );
    	
    	ResetXF (); // A0 = 0 (data register access)
    	asm( " NOP " ); asm( " NOP " ); asm( " NOP " ); // tAS >= 20 ns
    	return port0;
        }

    void Write( int addr, int value )
    {
    	ndelay( tCycle );
    	
        SetXF (); // A0 = 1 (addr register access)
    	asm( " NOP " ); asm( " NOP " ); asm( " NOP " ); // tAS >= 20 ns
        port0 = addr;
        
    	ndelay( tCycle );
    	
    	ResetXF (); // A0 = 0 (data register access)
    	asm( " NOP " ); asm( " NOP " ); asm( " NOP " ); // tAS >= 20 ns
    	port0 = value;
        }

    int ReadIndirect( int addr )
    {
        Read( addr );               // Read access to the target register (dummy read)
        return Read( R_INT_DATA );  // Read the target register value
        }

    void SelectFIFO( int fifo )
    {
        Write( R_FIFO, fifo );
        do ; while( Read( R_STATUS ) & M_BUSY );
        }

    void ResetFIFO( void )
    {
        Write( A_INC_RES_FIFO, M_RES_FIFO | M_RES_FIFO_ERR );
        do ; while( Read( R_STATUS ) & M_BUSY );
        }

    void IncF( void )
    {
        Write( A_INC_RES_FIFO, M_INC_F );
        do ; while( Read( R_STATUS ) & M_BUSY );
        }
    };

//////////////////////////////////////////////////////////////////////////////////////
// XHFC S/U Port
//
class XHFC_Port : XHFC_HW
{
    enum // instead of #defines
    {
        MAX_DFRAME_LEN_L1           =     64, // D-Channel buffer size

        // L1 States as in I.430
        //  
        TE_STA_RESET                =      0, //
        TE_STA_INACTIVE             =      1, // State F1: Inactive
        TE_STA_SENSING              =      2, // State F2: Sensing
        TE_STA_DEACTIVATED          =      3, // State F3: Deactivated
        TE_STA_AWAITING_SIGNAL      =      4, // State F4: Awaiting signal
        TE_STA_IDENTIFYING_INPUT    =      5, // State F5: Identifying input
        TE_STA_SYNCHRONIZED         =      6, // State F6: Synchronized
        TE_STA_ACTIVATED            =      7, // State F7: Activated
        TE_STA_LOST_FRAMING         =      8, // State F8: Lost framing

        // NT/LT L1 States as in I.430
        //  
        NT_STA_RESET                =      0, //
        NT_STA_DEACTIVATED          =      1, // State G1: Deactivated
        NT_STA_PENDING_ACTIVATION   =      2, // State G2: Pending activation
        NT_STA_ACTIVATED            =      3, // State G3: Activated
        NT_STA_PENDING_DEACTIVATION =      4, // State G4: Pending deactivation

        // I.430 Timers
        //  
        TIMER_T1                    =    100, // NT timer T1 (in ms)
        TIMER_T3                    =   8000, // Activation timer T3 (in ms)
        TIMER_T4                    =    500, // Dactivation timer T4 (in ms)

        // XHFC register values constants
        //  
        CLK_DLY_TE                  =   0x0e, // A_SU_CLK_DLY in TE mode
        CLK_DLY_NT                  =   0x6c, // A_SU_CLK_DLY in NT mode
        STA_ACTIVATE                =   0x60, // A_SU_WR_STA start activation
        STA_DEACTIVATE              =   0x40  // A_SU_WR_STA start deactivation
        };

    //////////////////////////////////////////////////////////////////////////////////

    class Timer
    {
        int counter; // == -1 means stopped, == 0 means expired, > 0 means counting
    
    public:

        Timer( void )
        {
            counter = -1; // Stopped
            }

        bool IsStopped( void ) const
        {
            return counter == -1;
            }

        void Start( int value )
        {
            if ( counter == -1 ) // Start only if not pending
                counter = value;
            }

        void Restart( int value )
        {
            counter = value;
            }

        void Stop( void )
        {
            counter = -1;
            }

        bool DecAndTestExpired( void )
        {
            if ( counter == -1 ) // Timer is stopped
                return false;
        
            if ( --counter > 0 ) // Timer is still running
                return false;

            // Timer expired
            counter = -1; // Stop timer
            return true;
            }
        };

    //////////////////////////////////////////////////////////////////////////////////

    int ID;                     // Port ID
    int max_Z;                  // FIFO depth (= 2^N) minus one

    struct
    {
        bool NT           : 1; // true if NT, false if TE
        bool Up           : 1; // true if Up, false if S0
        bool ExchPol      : 1; // true if reversed polarity
        bool IsActivated  : 1; // true if L1 is activated
        bool IsActivating : 1; // true if L1 is being activating
        } mode;

    // Cached chip registers
    //
    int su_ctrl0;
    int su_ctrl1;
    int su_ctrl2;
    int st_ctrl3;

    int fifo_irq; // Cached FIFO block interrupt status
    int fifo_irqmsk; // Cached FIFO block interrupt status

    // B-Channel related buffers
    //
    //unsigned char brx_buf[ 2 ][ B_CH_BUFSIZE ];
    //unsigned char btx_buf[ 2 ][ B_CH_BUFSIZE ];

    // D-Channel related buffers
    //
    unsigned char drx_buf[ MAX_DFRAME_LEN_L1 ];
    int drx_indx;
    int bytes2receive;

    unsigned char dtx_buf[ MAX_DFRAME_LEN_L1 ];  
    int dtx_indx;
    int dtx_append_indx;
    int bytes2transmit;

    // Layer 1 state & timers
    //
    int L1_state;
    Timer T3;
    Timer T4;
    Timer T1;

    //////////////////////////////////////////////////////////////////////////////////

private:

    // Setup FIFO using A_CON_HDLC, A_SUBCH_CFG, A_FIFO_CTRL
    // FIFO number is given always relative to port.
    //
    void ConfigFIFO( int fifo, int conhdlc, int subcfg, int fifoctrl, bool enable = true )
    {
        SelectFIFO( ID * 8 + fifo );
        Write( A_CON_HDLC, conhdlc );
        Write( A_SUBCH_CFG, subcfg );
        Write( A_FIFO_CTRL, fifoctrl );

        if ( enable )
            fifo_irqmsk |= ( 1 << fifo );
        else
            fifo_irqmsk &= ~( 1 << fifo );

        ResetFIFO ();
        SelectFIFO( ID * 8 + fifo );
#if 0
        tracef( "ConfigFIFO(): fifo = %c, conhdlc = %c, subcfg = %c, "
            "fifoctrl = %c, enable = %c",
            fifo, 
            ReadIndirect( A_CON_HDLC ), ReadIndirect( A_SUBCH_CFG ), 
            ReadIndirect( A_FIFO_CTRL ), enable );
#endif
        }

    // Line interface state change event handler 
    //
    void EH_StateChanged( void )
    {
        if ( mode.NT ) 
        {
            switch ( L1_state ) 
            {
                case NT_STA_DEACTIVATED: // G1
                    mode.IsActivated = false;
                    T1.Stop ();
                    // L1->L2: PH_DEACTIVATE | INDICATION
                    tracef( "%c L1->L2: D|I", ID );
                    break;

                case NT_STA_PENDING_ACTIVATION: // G2
                    T1.Restart( TIMER_T1 );
                    Write( R_SU_SEL, ID );
                    Write( A_SU_WR_STA, M_SU_SET_G2_G3 );
                    break;

                case NT_STA_ACTIVATED: // G3
                    mode.IsActivated = true;
                    T1.Stop ();
                    // L1->L2: PH_ACTIVATE | INDICATION
                    tracef( "%c L1->L2: A|I", ID );
                    break;

                case NT_STA_PENDING_DEACTIVATION: // G4
                    T1.Stop ();
                    break;
                }
            }
        else // mode TE
        {
            if ( L1_state <= TE_STA_DEACTIVATED || L1_state >= TE_STA_ACTIVATED )
            {
                mode.IsActivating = false;
                T3.Stop ();
                }

            switch ( L1_state ) 
            {
                case TE_STA_DEACTIVATED: // F3
                    if ( mode.IsActivated  )
                    {
                        mode.IsActivated = false;
                        T4.Start( TIMER_T4 );
                        }
                    break;

                case TE_STA_ACTIVATED: // F7
                    T4.Stop ();
                    
                    if ( mode.IsActivating )
                    {
                        mode.IsActivating = false;
                        mode.IsActivated = true;
                        // L1->L2: PH_ACTIVATE | CONFIRM
                        tracef( "%c L1->L2: A|C", ID );
                        } 
                    else 
                    {
                        if ( ! mode.IsActivated  )
                        {
                            mode.IsActivated = true;
                            // L1->L2: PH_ACTIVATE | INDICATION
                            tracef( "%c L1->L2: A|I", ID );
                            }
                        else
                        {
                            // L1 was already activated (e.g. F8->F7)
                            }
                        }
                    break;

                case TE_STA_LOST_FRAMING: // F8
                    T4.Stop ();
                    break;
                }
            }
        }

    // Read transparent audio data from FIFO
    //
    void EH_ReadFIFO_B_Channel( int bc )
    {
        SelectFIFO( ID * 8 + bc * 2 + 1 + M_REV );
        int rcnt = Read( A_USAGE ) - 1;
        
        const int B_CH_BUFSIZE = 8;
        
        if ( rcnt < B_CH_BUFSIZE ) 
        {
            // Not enough data in FIFO
            //
            // tracef( "Warning: FIFO fill level is too low" );

            //memset( brx_buf[ bc ], 0xFF, B_CH_BUFSIZE );
            return;
            }

        if ( rcnt > 2 * B_CH_BUFSIZE ) 
        {
            // Too much data in FIFO, so reduce it to 2 * B_CH_BUFSIZE
            //
            // tracef( "Warning: FIFO fill level is too high" );
            
            int mumbojumbo = rcnt - 2 * B_CH_BUFSIZE; 
            rcnt -= mumbojumbo;
            while ( mumbojumbo-- )
                Read( A_FIFO_DATA );

            SelectFIFO( ID * 8 + bc * 2 + 1 + M_REV );
            }

        for ( int i = 0; i < B_CH_BUFSIZE; i++ )
        {
            /*brx_buf[ bc ][ i ] =*/ Read( A_FIFO_DATA );
            }
    
        SelectFIFO( ID * 8 + bc * 2 + 1 + M_REV );
        }

    // Write transparent audio data to FIFO
    //
    void EH_WriteFIFO_B_Channel( int bc )
    {
        SelectFIFO( ID * 8 + bc * 2 + M_REV );
    
        int free = max_Z - Read( A_USAGE );

		const int B_CH_BUFSIZE = 8;
		
        if ( free < B_CH_BUFSIZE )
        {
            tracef( "Warning: Critical FIFO overrun" );
            return;
            }

        for ( int i = 0; i < B_CH_BUFSIZE; i++ )
        {
            Write( A_FIFO_DATA, 0xFF ); // btx_buf[ bc ][ i ] );
            }
        }

    void EH_ReadFIFO_D_Channel( void )
    {
        // Select D-RX FIFO
        //
        SelectFIFO( ID * 8 + 5 );

	    // Check for RX FIFO overflow
        //
	    int fstat = Read( A_FIFO_STA );
	    if ( fstat & M_FIFO_ERR )
        {
		    Write( A_INC_RES_FIFO, M_RES_FIFO_ERR ); // Reset error
		    }

        // HDLC rcnt
        //
        int f1 = Read( A_F1 );
        int f2 = Read( A_F2 );
        int z1 = Read( A_Z1 );
        int z2 = Read( A_Z2 );

        int rcnt = ( z1 - z2 ) & max_Z;
        if ( f1 != f2 )
            rcnt++; 

        if ( rcnt <= 0 ) 
            return;

        int& idx = drx_indx;

        // Read data from FIFO
        //
        for ( int i = 0; i < rcnt; i++ )
        {
            if (  idx < MAX_DFRAME_LEN_L1 )
                drx_buf[ idx++ ] = Read( A_FIFO_DATA );
            else
                Read( A_FIFO_DATA ); // Dummy read
            }

        if ( f1 == f2 ) 
            return;

        // HDLC frame termination
        //
        IncF ();

        // Check minimum frame size
        //
        if ( idx < 4 ) 
        {
            tracef( "Error: Frame < minimum size" );
            idx = 0;
            return;
            }

        // Last octet is STAT, which is 0x00 if CRC is OK.
        //
        if ( drx_buf[ idx - 1 ] == 0xFF )
        {
            tracef( "Error: Frame abort received" );
            idx = 0;
            return;
            }
        else if ( drx_buf[ idx - 1 ] != 0x00 )
        {
            tracef( "Error: CRC error" );
            idx = 0;
            return;
            }

        bytes2receive = idx - 3;
        idx = 0;

        tracef( "%c D-RX %c %a",
            ID, bytes2receive, drx_buf, bytes2receive );

        bytes2receive = 0; // FIXME: remove this in actual handler
        }

    void EH_WriteFIFO_D_Channel( void )
    {
        if ( ! bytes2transmit || ! mode.IsActivated )
            return;

        int& len = bytes2transmit;    // HDLC package len
        int& idx = dtx_indx;          // Already transmitted

        SelectFIFO( ID * 8 + 4 );

        Read( A_FIFO_STA );
        int free = max_Z - Read( A_USAGE );
        int tcnt = free >= len - idx ? len - idx : free;

        int f1 = Read( A_F1 );
        int f2 = Read( A_F2 );
        int fcnt = 0x07 - ((f1 - f2) & 0x07); // Free frame count in TX FIFO

	    // Check for TX FIFO underrun during frame transmission
        //
	    int fstat = Read( A_FIFO_STA );
	    if ( fstat & M_FIFO_ERR )
        {
		    Write( A_INC_RES_FIFO, M_RES_FIFO_ERR ); // Reset error
            idx = 0; // Restart frame transmission
            return;
		    }

        if ( ! free || ! fcnt || ! tcnt ) 
            return;

        // Write data to FIFO
        //
        for ( int i = 0; i < tcnt; i++ )
            Write( A_FIFO_DATA, dtx_buf[ idx++ ] );
        
        if ( idx != len) 
            return;

        // Terminate frame
        //
        IncF ();

	    // Check for TX FIFO underrun during frame transmission
        //
	    fstat = Read( A_FIFO_STA );
	    if ( fstat & M_FIFO_ERR )
        {
		    Write( A_INC_RES_FIFO, M_RES_FIFO_ERR ); // Reset error
            idx = 0; // Restart frame transmission
            return;
		    }

        len = 0;
        idx = 0;

        // TX completed. Get next frame to transmit if any?
        }

    //////////////////////////////////////////////////////////////////////////////////

public:

    bool IsActivated( void ) const
    {
        return mode.IsActivated;
        }

    bool IsActivating( void ) const
    {
        return mode.IsActivating;
        }

    XHFC_Port( void )
    {
        // All init is done in Initialize()
        }

    // Init line interface and port ID
    //
    void Initialize( int pt, int fifo_depth, bool NT_mode, bool Up_mode )
    {
        ID                  = pt;

        mode.NT             = NT_mode;
        mode.Up             = Up_mode;
        mode.ExchPol        = false;
        mode.IsActivated    = false;
        mode.IsActivating   = false;

        max_Z               = fifo_depth - 1;

        su_ctrl0            = 0;
        su_ctrl1            = 0;
        su_ctrl2            = 0;
        st_ctrl3            = 0;

        fifo_irq            = 0;
        fifo_irqmsk         = 0;

        drx_indx            = 0;
        bytes2receive       = 0;

        dtx_indx = 0;
        dtx_append_indx     = 0;
        bytes2transmit      = 0;

        L1_state            = 0;

        // Initialize S/U registers
        //
        Write( R_SU_SEL, ID );

        if ( mode.NT )
            su_ctrl0 |= M_SU_MD;

        if ( mode.ExchPol )
            su_ctrl2 |= M_SU_EXCHG;

        if ( mode.Up )
        {
            st_ctrl3 |= M_ST_SEL;
            Write( A_MS_TX, M_MS_TX );
            su_ctrl0 |= M_ST_SQ_EN;
            }

        Write( A_ST_CTRL3, st_ctrl3 );
        Write( A_SU_CTRL0, su_ctrl0 );
        Write( A_SU_CTRL1, su_ctrl1 );
        Write( A_SU_CTRL2, su_ctrl2 );
    
        Write( A_SU_CLK_DLY, mode.NT ? CLK_DLY_NT : CLK_DLY_TE );

        // Enable L1 state machine
        //  
        Write( R_SU_SEL, ID );
        Write( A_SU_WR_STA, 0 );

#if 1
        tracef( "Port::Initialize(ID=%c,NT=%c,U=%c): st ctrl3 = %c, su ctrl0 = %c, su ctrl1 = %c, su ctrl2 = %c",
            ID, NT_mode, Up_mode,
            st_ctrl3, su_ctrl0, su_ctrl1, su_ctrl2
            );
#endif
        }

    void Startup( void )
    {
        // Setup B-Channel buffers
        //
        for ( int bc = 0; bc < 2; bc++ ) 
        {
            //memset( btx_buf[ bc ], 0xFF, sizeof( btx_buf[ bc ] ) );
            //memset( brx_buf[ bc ], 0xFF, sizeof( brx_buf[ bc ] ) );
   
            ConfigFIFO( bc * 2,     6, 0, 0 ); // Enable B-Channel TX FIFO
            ConfigFIFO( bc * 2 + 1, 6, 0, 0 ); // Enable B-Channel RX FIFO
            Enable_B_Channel( bc );
            }

        // Setup D-Channel buffers
        //
        dtx_indx = 0;
        dtx_append_indx = 0;
        bytes2transmit = 0;
        drx_indx = 0;
        bytes2receive = 0;

        ConfigFIFO( 4, 5, 2, M_FR_ABO ); // Enable D-Channel TX FIFO
        ConfigFIFO( 5, 5, 2, M_FR_ABO | M_FIFO_IRQMSK ); // Enable D-Channel RX FIFO
        }

    void Enable_B_Channel( int bc )
    {
        if ( bc == 0 ) 
        {
            su_ctrl2 |= M_B1_RX_EN;
            su_ctrl0 |= M_B1_TX_EN;
            }
        else if ( bc == 1 )
        {
            su_ctrl2 |= M_B2_RX_EN;
            su_ctrl0 |= M_B2_TX_EN;
            } 
        else 
        {
            // Invalid B-Channel
            return;
            }

        Write( R_SU_SEL, ID );
        Write( A_SU_CTRL0, su_ctrl0 );
        Write( A_SU_CTRL2, su_ctrl2 );
        }

    void Disable_B_Channel( int bc )
    {
        if ( bc == 0 ) 
        {
            su_ctrl2 &= ~M_B1_RX_EN;
            su_ctrl0 &= ~M_B1_TX_EN;
            }
        else if ( bc == 1 )
        {
            su_ctrl2 &= ~M_B2_RX_EN;
            su_ctrl0 &= ~M_B2_TX_EN;
            } 
        else 
        {
            // Invalid B-Channel
            return;
            }

        Write( R_SU_SEL, ID );
        Write( A_SU_CTRL0, su_ctrl0 );
        Write( A_SU_CTRL2, su_ctrl2 );
        }

    void Loop_B_Channel( int bc )
    {
        ConfigFIFO( bc * 2,     0xC6, 0, 0, false );  // Connect B-Ch S/U RX with PCM TX, no irqs
        ConfigFIFO( bc * 2 + 1, 0xC6, 0, 0, false );  // Connect B-Ch S/U TX with PCM RX, no irqs

        Write( R_SLOT,   ID * 8 + bc * 2 );            // PCM timeslot B-Ch TX
        Write( A_SL_CFG, ID * 8 + bc * 2 + 0x80 );     // Enable B-Ch TX timeslot on STIO1

        Write( R_SLOT,   ID * 8 + bc * 2 + 1 );        // PCM timeslot B-Ch RX
        Write( A_SL_CFG, ID * 8 + bc * 2 + 1 + 0xC0 ); // Enable B-Ch RX timeslot on STIO1

        Enable_B_Channel( bc );
        }

    // L2->L1 D-Channel hardware access:
    //
    void PH_ActivateRequest( void )
    {
        tracef( "%c L2->L1: A|R", ID );

        if ( mode.NT )
        {
            Write( R_SU_SEL, ID );
            Write( A_SU_WR_STA, STA_ACTIVATE | M_SU_SET_G2_G3 );
            }
        else // mode TE
        {
            if ( ! mode.IsActivated  )
            {
                T3.Start( TIMER_T3 );
                mode.IsActivating = true;
                Write( R_SU_SEL, ID );
                Write( A_SU_WR_STA, STA_ACTIVATE );
                }
            else
            {
                // L1->L2: PH_ACTIVATE | CONFIRM
                tracef( "%c L1->L2: A|C", ID );
                } 
            }
        }

    void PH_DeactivateRequest( void )
    {
        tracef( "%c L2->L1: D|R", ID );

        if ( mode.NT )
        {
            Write( R_SU_SEL, ID );
            Write( A_SU_WR_STA, STA_DEACTIVATE );
            }
        else // mode TE
        {
            // No deactivate request in TE mode!
            } 
        }

    void D_TX_Append( char* data, int len )
    {
        if ( bytes2transmit > 0 )
            return;
    
        for ( int& i = dtx_append_indx; len > 0 && i < MAX_DFRAME_LEN_L1; i++, len-- )
            dtx_buf[ i ] = *data++;
        }

    void D_TX_Append( int octet )
    {
        if ( bytes2transmit > 0 )
            return;
    
        if ( dtx_append_indx < MAX_DFRAME_LEN_L1 )
            dtx_buf[ dtx_append_indx++ ] = octet;
        }

    void D_TX_Query( void )
    {
        if ( bytes2transmit > 0 )
            return;

        tracef( "%c D-TX %c %a", ID, dtx_append_indx, dtx_buf, dtx_append_indx );
        }

    void D_TX_Abort( void )
    {
        if ( bytes2transmit > 0 )
            return;

        dtx_append_indx = 0;
        }

    void D_TX_Send( void )
    {
        if ( bytes2transmit > 0 )
            return;

        bytes2transmit = dtx_append_indx;
        dtx_append_indx = 0;
        }

    void UpdateState( int new_state )
    {
        if ( new_state == L1_state ) 
            return;
        
        tracef( "%c STAT %c -> %c", ID, L1_state, new_state );

        L1_state = new_state;

        EH_StateChanged ();
        }

    bool UpdateFifoIrq( void )
    {
        fifo_irq |= Read( R_FIFO_BL0_IRQ + ID );
        return fifo_irq != 0;
        }

    //////////////////////////////////////////////////////////////////////////////////
    // Event Handlers

    void EH_TX_FIFOs( void )
    {
		// Handle B1 TX FIFO
        //
        if ( M_FIFO0_TX_IRQ & fifo_irqmsk )
        {
            fifo_irq &= ~M_FIFO0_TX_IRQ;
            EH_WriteFIFO_B_Channel( 0 );
            }

		// Handle B2 TX FIFO
        //
        if ( M_FIFO1_TX_IRQ & fifo_irqmsk )
        {
            fifo_irq &= ~M_FIFO1_TX_IRQ;
            EH_WriteFIFO_B_Channel( 1 );
            }

		// Handle D TX FIFO
        //
        if ( M_FIFO2_TX_IRQ & fifo_irqmsk )
        {
            fifo_irq &= ~M_FIFO2_TX_IRQ;
            EH_WriteFIFO_D_Channel ();
            }
        }

    void EH_RX_FIFOs( void )
    {
	    // Set fifo_irq when RX data is over treshold
        //
		fifo_irq |= Read( R_FILL_BL0 + ID );

		// Handle B1 TX FIFO
        //
        if ( M_FIFO0_RX_IRQ & fifo_irq & fifo_irqmsk )
        {
            fifo_irq &= ~M_FIFO0_TX_IRQ;
            EH_ReadFIFO_B_Channel( 0 );
            }

		// Handle B2 TX FIFO
        //
        if ( M_FIFO1_RX_IRQ & fifo_irq & fifo_irqmsk )
        {
            fifo_irq &= ~M_FIFO1_RX_IRQ;
            EH_ReadFIFO_B_Channel( 1 );
            }

		// Handle D TX FIFO
        //
        if ( M_FIFO2_RX_IRQ & fifo_irq & fifo_irqmsk )
        {
            fifo_irq &= ~M_FIFO2_RX_IRQ;
            EH_ReadFIFO_D_Channel ();
            }
        }

    void EH_TimerTicks( void )
    {
        if ( mode.NT && T1.DecAndTestExpired () )
        {
            tracef( "%c T1 expired %c", ID, L1_state );
            switch ( L1_state ) 
            {
                case NT_STA_DEACTIVATED: // G1
                    mode.IsActivated = false;
                    // L1->L2: PH_DEACTIVATE | INDICATION
                    tracef( "%c L1->L2: D|I", ID );
                    break;

                case NT_STA_PENDING_ACTIVATION: // G2
                    Write( R_SU_SEL, ID );
                    Write( A_SU_WR_STA, STA_DEACTIVATE );
                    break;

                case NT_STA_ACTIVATED: // G3
                    mode.IsActivated = true;
                    // L1->L2: PH_ACTIVATE | INDICATION
                    tracef( "%c L1->L2: A|I", ID );
                    break;

                case NT_STA_PENDING_DEACTIVATION: // G4
                    break;
                }
            }

        if ( T3.DecAndTestExpired () )
        {
            tracef( "%c T3 expired", ID );

            mode.IsActivating = false;
            Write( R_SU_SEL, ID );
            Write( A_SU_WR_STA, STA_DEACTIVATE );

            // L1->L2: PH_DEACTIVATE | INDICATION
            tracef( "%c L1->L2: D|I", ID );
            }

        if ( T4.DecAndTestExpired () )
        {
            tracef( "%c T4 expired", ID );

            // L1->L2: PH_DEACTIVATE | INDICATION
            tracef( "%c L1->L2: D|I", ID );
            }
        }

    };

//////////////////////////////////////////////////////////////////////////////////////
// XHFC Controller
//
class XHFC : public XHFC_HW
{
    enum // instead of #defines
    {
        MAX_XHFC_SU_PORTS = 2   // Max number of S0/Up ports per XHFC IC
        };

    //////////////////////////////////////////////////////////////////////////////////

    int chip_id;                // CHIP identifier
    int num_ports;              // Number of S and U interfaces

    // Statistics
    //
    unsigned long irq_cnt;      // Count interrupts

    unsigned long f0_accu;      // Accumulated F0IO (every 125us) pulse counter
    unsigned short f0_cnt;      // Last F0IO pulse counter value

    // Cached chip registers
    //
    int irq_ctrl;               // Interrupt control register
    int misc_irq;               // Miscellaneous interrupt status bits
    int misc_irqmsk;            // Mask of enabled misc interrupts
    int su_irq;                 // State change interrupt status bits
    int su_irqmsk;              // Mask of enables state change interrupts

public:

    XHFC_Port port[ MAX_XHFC_SU_PORTS ];

public:

    unsigned long GetIrqCount( void ) const
    {
        return irq_cnt;
        }

    unsigned long GetF0Count( void ) const
    {
        return f0_accu;
        }

    XHFC( void )
    {
        // Initialize performs initialization
        }

    // Initialize the XHFC ISDN Chip
    //
    bool Initialize( int NT_modes = 0, int Up_modes = 0 )
    {
        num_ports   = 0;

        irq_cnt     = 0;
        f0_accu     = 0;
        f0_cnt      = 0;

        irq_ctrl    = 0;
        misc_irqmsk = 0;
        misc_irq    = 0;
        su_irq      = 0;
        su_irqmsk   = 0;

        // Detect XHFC controller
        //
        chip_id = Read( R_CHIP_ID );
        
        tracef( "Chip ID %c", chip_id );

        // Configure FIFO depth
        //
        int fifo_depth = 0;
        switch ( chip_id ) 
        {
            case CHIP_ID_1SU: // Set 4 FIFOs with 256 bytes depth for TX and RX each
                num_ports = 1;
                fifo_depth = 256;
                su_irqmsk = M_SU0_IRQMSK;
                Write( R_FIFO_MD, M1_FIFO_MD * 2 );
                break;

            case CHIP_ID_2SU: // Set 8 FIFOs with 128 bytes depth for TX and RX each
                num_ports = 2;
                fifo_depth = 128;
                su_irqmsk = M_SU0_IRQMSK | M_SU1_IRQMSK;
                Write( R_FIFO_MD, M1_FIFO_MD * 1 );
                break;

            case CHIP_ID_2S4U:
            case CHIP_ID_4SU: // Set 16 FIFOs with 64 bytes depth for TX and RX each
                num_ports = 4;
                fifo_depth = 64;
                su_irqmsk = M_SU0_IRQMSK | M_SU1_IRQMSK | M_SU2_IRQMSK | M_SU3_IRQMSK;
                Write( R_FIFO_MD, M1_FIFO_MD * 0 );
                break;

            default:
                tracef( "ERROR: Initialize(): Unknown Chip ID 0x%c", chip_id );
                return false;
            }

        // Software reset to enable R_FIFO_MD setting
        //
        Write( R_CIRM, M_SRES );  // Soft reset (reset group 0)
        ndelay( 500 );            // Wait at least 5 us
        Write( R_CIRM, 0 );       // Deactivate reset

        // Set FIFO threshold
        //
        Write( R_FIFO_THRES, M1_THRES_TX | M1_THRES_RX ); // 16 bytes both TX & RX

        // Wait initialization sequence to complete
        //
        unsigned int timeout = 0x2000;

        while ( timeout && ( Read( R_STATUS ) & ( M_BUSY | M_PCM_INIT ) ) )
        {
            timeout--;
            }

        tracef( "hfc: Status %c, after t = %x", Read( R_STATUS ), 0x2000 - timeout );
        
        if ( timeout == 0 )
        {
            tracef( "hfc: Initialization sequence failed to complete." );
            return false;
            }

        // Configure PCM
        //
        int pcm_md0 = M_PCM_MD;                        // PCM master mode
        Write( R_PCM_MD0, pcm_md0 );
        Write( R_PCM_MD0, pcm_md0 + IDX_PCM_MD1 * M1_PCM_IDX ); // R_PCM_MD1 register accessible
        Write( R_PCM_MD1, 3 * M1_PLL_ADJ );            // Set PLL_ADJ = 3

        // Init line interfaces
        //
        for ( int pt = 0; pt < num_ports; pt++) 
        {
            port[ pt ].Initialize( 
                pt, fifo_depth,
                NT_modes & ( 1 << pt ) ? true : false, 
                Up_modes & ( 1 << pt ) ? true : false
                );
            }

        EnableInterrupts ();

        // Force initial L1 state changes
        //
        su_irq |= su_irqmsk;
/*
        // Configure PWM0 & PWM1
        //
        Write( R_PWM_CFG, 2 * M1_PWM_FRQ ); // PWM frequency is fSYS/64 = 64 kHz
        Write( R_PWM_MD, 1 * M1_PWM1_MD + 1 * M1_PWM0_MD ); // PWM0 & PWM1 mode: push-pull
        Write( R_PWM0,   0x30 ); // Set PWM0 duty cycle (0x80 == 50%)
        Write( R_PWM1,   0x30 ); // Set PWM1 duty cycle (0x80 == 50%)
*/
        // Enable GPIO2 & GPIO3 as outputs (LEDs)
        //
        Write( R_GPIO_SEL, M_GPIO_SEL0 | M_GPIO_SEL1 | M_GPIO_SEL2 | M_GPIO_SEL3 );
        Write( R_GPIO_EN0, M_GPIO_EN0  | M_GPIO_EN1  | M_GPIO_EN2  | M_GPIO_EN3  );

        return true;
        }

    // Disable all interrupts by disabling M_GLOB_IRQ_EN
    //
    void DisableInterrupts( void )
    {
        irq_ctrl &= ~M_GLOB_IRQ_EN;
        Write( R_IRQ_CTRL, irq_ctrl );
        }

    // Start interrupt and set interrupt mask
    //
    void EnableInterrupts( void )
    {
        Write( R_SU_IRQMSK, su_irqmsk );
/*
        // Set timer interrupt
        //
        Write( R_TI_WD, 0x02 ); // 1 ms interval
        misc_irqmsk |= M_TI_IRQMSK;
        Write( R_MISC_IRQMSK, misc_irqmsk );
*/
        // Clear all pending interrupts bits
        //
        Read( R_MISC_IRQ );
        Read( R_SU_IRQ );
        Read( R_FIFO_BL0_IRQ );
        Read( R_FIFO_BL1_IRQ );
        Read( R_FIFO_BL2_IRQ );
        Read( R_FIFO_BL3_IRQ );

        // Enable global interrupts
        //
        irq_ctrl |= M_GLOB_IRQ_EN | M_FIFO_IRQ_EN;
        Write( R_IRQ_CTRL, irq_ctrl );
        }

    void EH_Timer( void )
    {
        // Handle TX FIFO events
        //
        for ( int pt = 0; pt < num_ports; pt++ )
        {
            port[ pt ].EH_TX_FIFOs ();
            }
 
        for ( int pt = 0; pt < num_ports; pt++ )
        {
            port[ pt ].EH_TimerTicks ();
            }
        }
        
    void EH_Interrupt( void )
    {
		if ( ! IsIrqAsserted () )
			return;

        if ( ! ( irq_ctrl & M_GLOB_IRQ_EN ) ) // IRQs are not enabled at all
            return;

        if ( ! Read( R_IRQ_OVIEW ) ) // If not indicated any interrupt
            return;
        
        // Update IRQ state bits
        //
        bool schedule_BH = false;

        if ( misc_irq |= Read( R_MISC_IRQ ) )
            schedule_BH = true;

        if ( su_irq |= Read( R_SU_IRQ ) )
            schedule_BH = true;

        for ( int pt = 0; pt < num_ports; pt++ ) 
        {
            if ( port[ pt ].UpdateFifoIrq () )
                schedule_BH = true;
            }

		if ( ! schedule_BH ) // No need to schedule bottom half irq handler
            return;

        // Count interrupts.
        //
        ++irq_cnt;

        // Accumulate F0 counter
        //
        unsigned short cnt = Read( R_F0_CNTL );
        cnt += ( Read( R_F0_CNTH ) << 8 );

        int f0_delta = int( cnt - f0_cnt );
        if ( f0_delta > 0 )
        {
            f0_accu += f0_delta;
            }
        else if ( f0_delta < 0 )
        {
            f0_accu += f0_delta + 0xFFFF;
            }
            
        f0_cnt = cnt;

        // Handle RX FIFOs events
        //
	    for ( int pt = 0; pt < num_ports; pt++ )
        {
            port[ pt ].EH_RX_FIFOs ();
            }

        // Handle S/U state change events
        //
	    for ( int pt = 0; pt < num_ports; pt++ )
        {
            // Handle S/U state change interrupts
            //
            if ( su_irq & ( 1 << pt ) ) 
            {
                su_irq &= ~( 1 << pt );

                Write( R_SU_SEL, pt );
                port[ pt ].UpdateState( Read( A_SU_RD_STA ) & M_SU_STA );

                // Update LEDs
                //
                Write( R_GPIO_OUT0, 
                      ( port[ 0 ].IsActivated () ? M_GPIO_OUT2 : 0 ) // Green
                    | ( port[ 1 ].IsActivated () ? M_GPIO_OUT3 : 0 )
                    | ( port[ 0 ].IsActivated () ? M_GPIO_OUT0 : 0 ) // Yellow
                    | ( port[ 1 ].IsActivated () ? M_GPIO_OUT1 : 0 )
                    );
                }
            }
        }
        
	int Get_R_STATUS () { return Read( R_STATUS ); }
	int Get_R_IRQ_OVERVIEW () { return Read( R_IRQ_OVIEW ); }
    };

#endif // _XHFC_H_INCLUDED
