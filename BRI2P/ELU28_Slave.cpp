
#include "DSP.h"
#include "MsgBuf.h"
#include "ELU28.h"

void ELU28_D_Channel::Slave_RcvBuf_EH( void )
{
    int octet = rcv_buf.GetOctet ();
    if ( octet < 0 )
    	return;

    // Event: ON RECEIVED OCTET

    switch ( state )
    {
        case DISABLED:
        case WAIT_LOOP_SYNC:
        case DELAYED_POWER_UP:
            // ignore garbage
            break;
            
        case WAIT_SIGNAL_INQUIRY:
            if ( octet != 0x00 ) // If garbage received
            {
                poll_counter = 0; // reset counter
        		// Same state, continue timeout counter
                }
            else if ( ++poll_counter >= 5 ) // n consequtive Signal Inquiries
            {
                // Trigger: ON LSD ACTIVE
                //
                Go_State( IDLE, -1 );
				SetVerb_State( VERBOSE_UP );

				if ( delayed_power_up )
				{
				    if ( EQUSTA_len > 0 )
						xmt_que.PutPDU( EQUSTA, EQUSTA_len );
	
			    	delayed_power_up = false;
				    EQUSTA_len = 0;
					}
				else
				{
					// Now, report SYNC OK to the host
					//        
			        packet[ 0 ] = 0x00; // OPC = 0x00
			        packet[ 1 ] = 0x51; // FNC = 0x51
			        packet[ 2 ] = 0x01; // param = LS up, passive
			        outBuf.PutPack( MSGTYPE_IO_D, packet_header, 3 + 1 );
			        }
                }
            break;

        case IDLE:
            if ( octet == 0x00 ) // Signal Inquiry and Transmission
            {
                if ( xmt_que.IsQueueEmpty () )
                {
                    xmt_que.SendNegativePollAnswer ();
        			// Same state, continue timeout counter
                    }
                else
                {
                    // Send Signal
                    xmt_que.StartTransmission ();

					YellowLED.TurnOn ();

                	Go_State( TRANSMITTING_PDU, 1000 ); // Transmission rate is ~1.6kBy/s
                    }
                }
            else if ( octet >= 4 && octet <= 127 )
            {
                NBYTES = octet;
                packet_len = 0;
                packet_status = PACKET_INCOMPLETE;
                rcvd_octets = 1;
                cksum = octet;
                
                Go_State( RECEIVING_PDU, 3 ); // 2ms timeout
                }
            else if ( octet == 0x80 || octet == 0x81 )
            {
                NBYTES = ( octet - 0x80 ) << 8;
                packet_len = 0;
                packet_status = PACKET_INCOMPLETE;
                rcvd_octets = 1;
                cksum = octet;
                
                Go_State( WAIT_NBYTES_LOW, 3 ); // 2ms timeout
                }
            else
            {
                Go_State( IDLE, -1 );
            	}
            break;

        case WAIT_NBYTES_LOW:
            NBYTES += octet;
            if ( NBYTES >= 5 && NBYTES < 305 )
            {
                cksum += octet;
            	rcvd_octets ++;
                
                Go_State( RECEIVING_PDU, 3 ); // 2ms timeout
                }
            else
            {
                Go_State( IDLE, -1 );
                }
            break;

        case RECEIVING_PDU:
        
	    	YellowLED.SetCadence( 1, 10, 0 );
	    	
            if ( ++rcvd_octets < NBYTES )
            {
                if ( packet_len < sizeof( packet ) / sizeof( packet[0] ) )
                    packet[ packet_len++ ] = octet;
                else
                	packet_status = PACKET_OVERFLOW;
                cksum += octet;
                
                Go_State( RECEIVING_PDU, 3 ); // 2ms timeout
                }
            else // Last byte i.e. checksum
            {
                cksum = ( cksum - 1 ) & 0xFF;
                if ( cksum != octet ) // checksum is NOT OK
                {
                    xmt_que.SendNegativeAck ();
                    
                    // ignore incoming signal
                	Go_State( IDLE, -1 );
                    }
                else
                {
                    xmt_que.SendPositiveAck ();
                    
                    int newSeqNo = ( packet[ 0 ] >> 1 ) & 0x7;
                    if ( newSeqNo == oldRcvdSeqNo 
                        && ( newSeqNo != 0 || cksum == oldChecksum )
                        )
                    {
                        // Ignore incoming signal
                		Go_State( IDLE, -1 );
                        }
                    else
                    {
                        oldRcvdSeqNo = newSeqNo;
                        oldChecksum = cksum;
                        
                        // Update enhanced protocol, if P == 1
                        xmt_que.SetEnhancedProtocol( ( packet[ 0 ] & 0x10 ) != 0 );
                        
						if ( packet_status != PACKET_OVERFLOW )
							packet_status = PACKET_COMPLETED;

                		Go_State( IDLE, -1 );
                        }
                    }
                }
            break;

        case WAIT_FOR_ACK:
            if ( octet == 0xAA )
            {
                xmt_que.ErasePDU ();
                
                Go_State( IDLE, -1 );
                }
            else // octet == 0x75, or other
            {
            	xmt_que.RestartTransmission ();

                Go_State( IDLE, -1 );
                }
            break;
            
         case TRANSMITTING_PDU:
         	// Protocol is full-duplex.
         	// We should not receive anything while transmitting.
        	// Do not change the state.
         	break;

        case WAIT_NBYTES:
            Freeze_CPU ();
            break;
        }
    }

void ELU28_D_Channel::Slave_Timed_EH( void )
{
	if ( state == TRANSMITTING_PDU && xmt_que.IsIdle () )
	{
		// Event: On Transmission Comleted         
		
		YellowLED.TurnOff ();

		Go_State( WAIT_FOR_ACK, 7 ); // Wait signal acknowledge, max 6ms
		}

    if ( timer == 0 )
    {
        // Event: ON TIMEOUT
        
        timer = -1; // acknowledge event i.e. disable timer
        
        switch( state )
        {
            case DISABLED:
                break;

			case WAIT_LOOP_SYNC:
				dasl.PowerDown ();
				Go_State( DISABLED, -1 );
				break;
				
            case WAIT_SIGNAL_INQUIRY:
                // Remain in the same state
                break;
                                
            case IDLE:
                Go_State( IDLE, -1 );
                break;
                                
            case WAIT_NBYTES_LOW:
                Go_State( IDLE, -1 );
                break;
                
            case RECEIVING_PDU:
                Go_State( IDLE, -1 );
                break;
                
            case WAIT_FOR_ACK:
            	xmt_que.RestartTransmission ();
            	
                Go_State( IDLE, -1 );
                break;

            case TRANSMITTING_PDU:
            	// Transmitter takes to long time to transmit signal
            	Freeze_CPU (); // severe error
            	break;

            case WAIT_NBYTES:
                Freeze_CPU ();
                break;
                
            case DELAYED_POWER_UP:
            
	        	if ( dasl.IsLineSignalDetected () )
	        	{
	        		dasl.PowerUp ();
	        		Go_State( WAIT_LOOP_SYNC, 200 );
			        SetVerb_State( VERBOSE_DOWN ); GreenLED.SetCadence( -1, 1, 19 );
	        		}
	        	else
	        	{
            		Go_State( DISABLED, -1 );
            		}
            	break;
            }
        }
    }

void ELU28_D_Channel::Slave_DASL_EH( void )
{
	dasl.Update ();
    	
    switch( state )
    {
        case DISABLED:
        	if ( dasl.IsLineSignalDetected () )
        	{
        		dasl.PowerUp ();
        		Go_State( WAIT_LOOP_SYNC, 200 );
		        SetVerb_State( VERBOSE_DOWN ); GreenLED.SetCadence( -1, 1, 19 );
        		}
            break;

        case WAIT_LOOP_SYNC:
        	if ( ! dasl.IsLineSignalDetected () )
        	{
        		dasl.PowerDown ();
        		Go_State( DISABLED, -1 );
		        SetVerb_State( VERBOSE_DOWN );
        		}
        	else  if ( dasl.IsLoopInSync () ) // ON LOOP IN SYNC
        	{
        		OnLoopInSync ();
        		
		        Go_State( WAIT_SIGNAL_INQUIRY, -1 ); // no timeouts
		        SetVerb_State( VERBOSE_HALFUP );
        		}
            break;

		case DELAYED_POWER_UP:
			// Ignore DASL state changes
			break;

        default:
        	if ( ! dasl.IsLoopInSync () ) // ON LOOP OUT OF SYNC
        	{
        		OnLoopOutOfSync ();

				if ( state != WAIT_SIGNAL_INQUIRY )
				{
					// Now, report LOST SYNC to the host
					//        
			        packet[ 0 ] = 0x00; // OPC = 0x00
			        packet[ 1 ] = 0x51; // FNC = 0x51
			        packet[ 2 ] = 0x00; // param = LS down
			        outBuf.PutPack( MSGTYPE_IO_D, packet_header, 3 + 1 );
		        	}
		        
				if ( dasl.IsLineSignalDetected () )
				{
            		Go_State( WAIT_LOOP_SYNC, 200 );
			        SetVerb_State( VERBOSE_DOWN ); GreenLED.SetCadence( -1, 1, 19 );
					}
				else
				{
            		dasl.PowerDown ();
            		Go_State( DISABLED, -1 );
			        SetVerb_State( VERBOSE_DOWN );
					}
        		}
        	break;
        }
	}
