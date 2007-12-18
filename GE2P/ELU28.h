#ifndef _ELU28_H_INCLUDED
#define _ELU28_H_INCLUDED

#include "Cadence.h"
#include "DASL.h"
#include "ELUFNC.h"
#include "B_Channel.h"

///////////////////////////////////////////////////////////////////////////////

class D_ReceiveBuffer
{
    volatile bool disabled;
    int bitCount;
    int lastBit;
    int* readp;
    int* writep;
    int* maxp;
    int bufp[ 32 ];

public:

    D_ReceiveBuffer( void )
    {
        disabled = true;
        bitCount = 0;
        lastBit = 0;
        readp = bufp;
        writep = bufp;
        maxp = bufp + sizeof( bufp ) / sizeof( bufp[0] ) - 1;
        }
        
    void Initialize( void )
    {
        bitCount = 0;
        lastBit = 0;
        readp = writep = bufp;
        disabled = false;
        }
        
    void Disable( void )
    {
        disabled = true;
        readp = writep = bufp;
        }

    inline int GetOctet( void )
    {
        if ( writep == readp )
            return -1;

        int ret = *readp++; // get value and advance tail pointer
        if ( readp > maxp )
            readp = bufp;
            
        return ret;
        }

    inline void PutBit( int inbit )
    {
        if ( disabled )
        {
        	// Ignore input
            }
        else if ( bitCount == 0 ) // If idle
        {
            if ( lastBit == 1 && inbit == 0 )
            {
                bitCount = 1; // Start receiver
                *writep = 0; // init data to blank
                }
            }
        else if ( bitCount <= 8 ) // Receiving octet bits
        {
            *writep >>= 1;
            *writep |= ( inbit << 7 );
            ++bitCount;
            }
        else // Expect to receive stop bit
        {
            if ( inbit == 0 ) // Still receiving zero; consider a 'break'
            {
                *writep |= 0x0100; // Mark 'break'
                // ... and continue waiting stop bit !!!
                }
            else // We have received a stop bit
            {
                bitCount = 0; // Go idle
                
                ++writep; // Advance head pointer
                if ( writep > maxp )
                    writep = bufp;
                                                                      
				// FIXME: MUTEX is needed!
				//                                                                      
                if ( writep == readp ) // If buffer overflow
                {
                    ++readp; // Advance tail pointer, i.e. overwrite old data
                    if ( readp > maxp )
                        readp = bufp;
                    }
                }
            }
            
        lastBit = inbit;
        }
    };
    
///////////////////////////////////////////////////////////////////////////////

class D_TransmitQueue
{
    volatile bool disabled;
    volatile int octetCount;
    int bitCount;

	int id;
    bool flowXON;
    
    unsigned short shiftReg;
    bool isLoadedShiftReg;
    
    int curSeqNo;
    int attempt_counter;
    bool enhanced_protocol;

    unsigned short* readp;
    unsigned short* writep;
    unsigned short* xmitp;
    unsigned short* maxp;
    unsigned short bufp[ 256 ]; // ~512 octets

public:

    unsigned short dropped_counter;
    
    D_TransmitQueue( int p_id )
    	: id( p_id )
    {
    	disabled = true;
    	flowXON = false;
    	octetCount = 0;
    	bitCount = 0;
    	curSeqNo = 0;
    	shiftReg = 0;
    	isLoadedShiftReg = false;
    	attempt_counter = 0;
    	enhanced_protocol = false;
    	dropped_counter = 0;
        readp = writep = xmitp = bufp;
        maxp = bufp + sizeof( bufp ) / sizeof( bufp[0] ) - 1;
        }
        
    void Disable( void )
    {
        disabled = true;
    	flowXON = false;
        octetCount = 0; // To signal IDLE state
		// SendHostFlowStatus (); Loop Sync LOST overrides any XON
        }
        
	void SendHostFlowStatus( void )
	{    	
		unsigned char pdu [] = { 0, 0x00, 0x52, 0 };
		pdu[ 0 ] = id;
		pdu[ 3 ] = flowXON;
		outBuf.PutPack( MSGTYPE_IO_D, pdu, sizeof( pdu ) );
		}
		
    void Initialize( void )
    {
    	octetCount = 0;
    	bitCount = 0;
    	curSeqNo = 0;
    	shiftReg = 0;
    	isLoadedShiftReg = false;
    	attempt_counter = 0;
    	enhanced_protocol = false;
    	dropped_counter = 0;
        readp = writep = xmitp = bufp;
    	disabled = false;
    	flowXON = true;
    	
		// SendHostFlowStatus (); Loop Sync OK overrides any XOFF
        }

	void SetEnhancedProtocol( bool value = true )
	{
		enhanced_protocol = value;
		}

	bool IsEnhancedProtocol( void ) const
	{
		return enhanced_protocol;
		}

    bool IsQueueEmpty( void ) const
    {
        return readp == writep;
        }
        
    bool IsIdle( void ) const
    {
        return ! disabled && octetCount == 0;
        }
        
    void SendSignalInquiry( void )
    {
    	if ( ! IsIdle () )
    		return;

    	isLoadedShiftReg = true;
    	shiftReg = 0x00;
    	octetCount = 1;
    	}
    	
    void SendNegativePollAnswer( void )
    {
    	if ( ! IsIdle () )
    		return;

    	isLoadedShiftReg = true;
    	shiftReg = enhanced_protocol ? 0x01 : 0x00;
    	octetCount = 1;
    	}
    	
    void SendPositiveAck( void )
    {
    	if ( ! IsIdle () )
    		return;

    	isLoadedShiftReg = true;
    	shiftReg = 0xAA;
    	octetCount = 1;
    	}
    	
    void SendNegativeAck( void )
    {
    	if ( ! IsIdle () )
    		return;

    	isLoadedShiftReg = true;
    	shiftReg = 0x75;
    	octetCount = 1;
    	}

    bool PutPDU( unsigned char* data, int len );
    
    void StartTransmission( void )
    {   
    	if ( ! IsIdle () || IsQueueEmpty () )
    		return;
    		
    	xmitp = readp;
    	
    	int len = *xmitp++;
    	if ( xmitp > maxp )
    		xmitp = bufp;

    	isLoadedShiftReg = false;
    	octetCount = len; // Start transmission
    	}

    void ErasePDU( void )
    {
    	if ( ! IsIdle () || IsQueueEmpty () )
    		return;

		attempt_counter = 0;

		readp = xmitp; // xmitp points to first word after transmit frame

		if ( ! flowXON )
		{		
		    // Check used space. If it drops bellow "low water mark" and
		    // it flowXON was false (i.e. it was XOFF state), then
		    // signal to host transition to XON state.
		    //
		    int used_space = writep >= readp 
		        ? writep - readp
		        : writep + sizeof( bufp ) / sizeof( bufp[0] ) - readp;
	        
	    	if ( used_space < 64 )
	    	{
	    		flowXON = true;
				SendHostFlowStatus ();
	    		}
	    	}
    	}
    
    void RestartTransmission( void )
    {   
    	if ( ! IsIdle () || IsQueueEmpty () )
    		return;
    		
    	if ( attempt_counter < 2 )
    		++attempt_counter;
		else
			ErasePDU ();
    	}
    
    inline int GetBit( void )
    {
        int outbit;
        
        if ( disabled || octetCount == 0 ) // If transmit buffer is empty or disabled
        {
            outbit = 1; // Idle bit
            }
        else if ( bitCount == 0 ) // If start bit to send
        {
            outbit = 0; // Start bit

            if ( ! isLoadedShiftReg ) // If shiftReg depleted, load it
            {
            	shiftReg = *xmitp++;
            	if ( xmitp > maxp )
            		xmitp = bufp;
            	}

            ++bitCount;
            }
        else if ( bitCount >= 9 ) // stop bit(s)
        {
            outbit = 1; // Stop bit
            bitCount = 0; // Next: Start bit
                                                  
			// isLoadedShiftReg indicates is there octet in bits 7-0
			// During start bit, if shiftReg is not loaded it will be
			// loaded with 16-bit word, i.e. two octets. Bits 7-0 will
			// contain MSB and 15-8 LSB octet.
			// 
			isLoadedShiftReg = ! isLoadedShiftReg;

            --octetCount;
            }
        else // data bit to send
        {
            outbit = shiftReg & 1;
            shiftReg >>= 1;
            ++bitCount; // next: Next data bit
            }
            
        return outbit;
        }
    };

///////////////////////////////////////////////////////////////////////////////

class ELU28_D_Channel
{
    enum STATE
    {
        DISABLED,          // DASL is in power down
        WAIT_LOOP_SYNC,    // DASL is in line-signal detected mode
        				   // Other: DASL is in sync
        WAIT_SIGNAL_INQUIRY,
        IDLE,
        WAIT_NBYTES,
        WAIT_FOR_ACK,
        WAIT_NBYTES_LOW,
        RECEIVING_PDU,
        TRANSMITTING_PDU,
        DELAYED_POWER_UP
        };
        
    int id;
    
    STATE state;

    volatile int timer; // general 1ms timeout/countdown timer;
                        // when -1 disabled; when 0 triggers ON TIMEOUT event

    int poll_counter; // number of consecutive polls (00h's)

	int timeout_counter;
	int fault_counter;
	
	int timeout_counter_poll;

	struct B_Channel* bChannel;
	
	enum VERBOSE_STATE
	{
		VERBOSE_DOWN			= 0,
		VERBOSE_HALFUP			= 1,
		VERBOSE_UP				= 2,
		VERBOSE_RINGING			= 3,
		VERBOSE_TRANSMISSION	= 4,
		VERBOSE_FAULTY_DTS      = 5
		};
		
	unsigned short verb_state;
	int transmission_order;
	bool IsOWS;
	bool IsCTT10;

    // Receiver State
    //
    int oldRcvdSeqNo;
    int oldChecksum;
    	
    int NBYTES;
    int rcvd_octets;
    int cksum;

private: // Methods

	///////////////////////////////////////////////////////////////////////////
	//
	void Go_State( STATE newState ) // without timeout
	{
		state = newState;
		}

	void Go_State( STATE newState, int timeout )
	{
		state = newState;
		timer = timeout;
		}

	///////////////////////////////////////////////////////////////////////////
	//
	void SetVerb_State( VERBOSE_STATE vs )
	{
		verb_state = vs;

		switch( vs )
		{
			case VERBOSE_DOWN:
				GreenLED.TurnOff ();
				YellowLED.SetCadence( -1, 850, 50, 50, 50 );
				break;
			case VERBOSE_HALFUP:
				GreenLED.SetCadence( -1, 50, 50 );
				YellowLED.TurnOff ();
				break;
			case VERBOSE_UP:
				GreenLED.TurnOn ();
				break;
			case VERBOSE_TRANSMISSION:
        		GreenLED.SetCadence( -1, 500, 500 );
        		break;
			case VERBOSE_RINGING:
				GreenLED.SetCadence( -1, 200, 200, 200, 200 );
				break;
			case VERBOSE_FAULTY_DTS:
				GreenLED.SetCadence( -1, 50, 50 );
				YellowLED.SetCadence( -1, 50, 50 );
				break;
			}
		}
public:

	int timeout_counter_nbytes;
	int timeout_counter_pdu;
	int timeout_counter_ack;
	
    DASL dasl;

    D_TransmitQueue xmt_que;
    D_ReceiveBuffer rcv_buf;

	Cadence GreenLED;
	Cadence YellowLED;
	
    // Receiver
    //
    enum PACKET_STATUS
    {
    	PACKET_EMPTY, PACKET_INCOMPLETE, PACKET_COMPLETED, PACKET_OVERFLOW 
    	};
    	
   	PACKET_STATUS packet_status;
    unsigned char packet_header[ 1 ];
    unsigned char packet[ 310 ];
    int packet_len;
    
    unsigned char EQUSTA[ 64 ];
    int EQUSTA_len;
    bool delayed_power_up;
    
	///////////////////////////////////////////////////////////////////////////
	//
    ELU28_D_Channel( int p_id, B_Channel* bChannel );
    void Initialize( void );

    void Master_RcvBuf_EH( void );
    void Slave_RcvBuf_EH( void );
    
    void Master_Timed_EH( void );
    void Slave_Timed_EH( void );

    void Master_DASL_EH( void );
    void Slave_DASL_EH( void );

	void ParsePDU_FromPBX( unsigned char* data, int data_len );
	void ParsePDU_FromDTS( unsigned char* data, int data_len );
	
	int GetVerbState( void ) const
	{
		return verb_state;
		}
		
	int GetFaultCounter( void ) const
	{    
		return fault_counter;
		}
		
	int GetTimeoutCounter( void ) const
	{    
		return timeout_counter;
		}

	int GetDroppedCounter( void ) const
	{
		return xmt_que.dropped_counter;
		}

    void DecTimeoutTimer( void )
    {
    	if ( timer > 0 )
        	timer--;
       	}

	bool RcvBuf_EH( void )
	{       	
        if ( dasl.IsMaster () )
     		Master_RcvBuf_EH ();
     	else
	        Slave_RcvBuf_EH ();

        bool rc = packet_status == PACKET_COMPLETED;
        packet_status = PACKET_EMPTY;

        if ( rc )
        {
            outBuf.PutPack( MSGTYPE_IO_D, packet_header, packet_len + 1 );

			if ( dasl.IsMaster () )
				ParsePDU_FromDTS( packet, packet_len );
			else
				ParsePDU_FromPBX( packet, packet_len );
           	}

        return rc;
     	}

	void Timed_EH( void )
	{       	
        if ( dasl.IsMaster () )
     		Master_Timed_EH ();
     	else
	        Slave_Timed_EH ();
     	}

	void DASL_EH( void )
	{       	
        if ( dasl.IsMaster () )
     		Master_DASL_EH ();
     	else
	        Slave_DASL_EH ();
     	}

    void SetOWS( void )
    {
		IsOWS = true;

		if ( ! transmission_order )
		{ 
		    transmission_order = 0x01;
			bChannel->StartTransmission ();
			SetVerb_State( VERBOSE_TRANSMISSION );
			}
    }
    
    void OnLoopInSync( void )
    {
		bChannel->StopTransmission ();
		
		rcv_buf.Initialize ();
		xmt_que.Initialize ();
		xmt_que.SetEnhancedProtocol( false );

		oldRcvdSeqNo = 7;
		oldChecksum = 0;

		fault_counter = 0;
		timeout_counter = 0;
		timeout_counter_nbytes = 0;
		timeout_counter_pdu = 0;
		timeout_counter_ack = 0;

		packet_status = PACKET_EMPTY;
		packet_len = 0;        
		
		transmission_order = 0;
		IsOWS = false;
		IsCTT10 = false;
		poll_counter = 0;
		}

    void OnLoopOutOfSync( void )
    {
		bChannel->StopTransmission ();

		xmt_que.Disable ();
		rcv_buf.Disable ();
        }
   
    void DelayedPowerUp( void )
    {
    	delayed_power_up = true;
    	EQUSTA_len = 0;

		OnLoopOutOfSync ();
		dasl.PowerDown ();
		Go_State( DELAYED_POWER_UP, 2000 );
		SetVerb_State( VERBOSE_DOWN );
		}
    };

#endif // _ELU28_H_INCLUDED
