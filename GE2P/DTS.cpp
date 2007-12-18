
#include "DSP.h"
#include "MsgBuf.h"
#include "ELU28.h"

void ELU28_D_Channel::ParsePDU_FromDTS( unsigned char* data, int data_len )
{
    *data++; // Skip OPC
    --data_len;
    
    if ( data_len <= 0 )
        return;

    switch( *data )
    {
        case FNC_EQUSTA:
            //
            // Function: The signal sends status and equipment identity to the system
            // software. It is sent when the instrument has received an activation
            // signal if the initialisation test is without errors. It is also sent on a
            // special request or if a loop back order is received.
            // If the instrument is not in active mode, this signal is sent if the
            // instrument receives any signal or if any key or hook is pushed.
            // If transmission order = "Low speaking" is received in the instrument
            // when hook is on, the EQUSTA signal is sent back and the order is
            // ignored.
            //
            // Return signals: EQUACT or EQUSTAREQ or EQULOOP or DISTRMUPDATE
            //
            //  0  8, ! 01 H FNC (Function code)
            //  1  8, ! XX H EQUIPMENT TYPE
            //  2  8, ! XX H EQUIPMENT VERSION
            //  3  8, ! XX H LINE STATE
            //  4  8, ! XX H EQUIPMENT STATE
            //  5  8, ! XX H BIT FAULT COUNTER (not used)
            //  6  8, ! XX H BIT FAULT SECONDS (not used)
            //  7  8, ! XX H TYPE OF ACTIVITY (not used)
            //  8  8, ! XX H LOOP STATE
            //  9  8, ! XX H EXTRA VERSION INFORMATION
            // 10  8, ! XX H OPTION UNIT INFORMATION
            //
            if ( /*equState*/ ( data[ 4 ] & 0x3F ) == 0 ) // Passive
            {
            	IsOWS = false;
            	IsCTT10 = false;
            	transmission_order = 0;
            	bChannel->StopTransmission ();
    			SetVerb_State( VERBOSE_UP );
            	
        		// Remember EQUSTA to send, after delayed power up
				memcpy( EQUSTA, data - 1, data_len + 1 ); // include OPC
				EQUSTA_len = data_len + 1;
                }
            else // Active
            {
				if ( data[ 1 ] == 0x80 && data[ 2 ] == 0x02 ) // Is OWS
				{
					if ( ! IsOWS )
					{
						tracef( "%c: Set OWS *a", id );
						}
					
					IsOWS = true;
					
					if ( ! transmission_order )
					{
						transmission_order = 0x01;
	        			bChannel->StartTransmission ();
	        			SetVerb_State( VERBOSE_TRANSMISSION );
	        			}
	        			
	        		// Remember EQUSTA to send, after delayed power up
					memcpy( EQUSTA, data - 1, data_len + 1 ); // include OPC
					EQUSTA_len = data_len + 1;
					}
				else if ( data[ 1 ] == 0x01 ) // Is CTT10
				{
					if ( ! IsCTT10 )
					{
						tracef( "%c: Set CTT10 *a", id );
						}
					
					IsCTT10 = true;

					if ( data[ 2 ] == 0x01 ) // Inactive CTT10
					{
		            	transmission_order = 0;
		            	bChannel->StopTransmission ();
	        			SetVerb_State( VERBOSE_UP );
		            	}
		            else if ( data[ 2 ] == 0x03 ) // Active CTT10
		            {
						if ( ! transmission_order )
						{
							transmission_order = 0x01;
		        			bChannel->StartTransmission ();
		        			SetVerb_State( VERBOSE_TRANSMISSION );
		        			}
	        			}

	        		// Remember EQUSTA to send, after delayed power up
					memcpy( EQUSTA, data - 1, data_len + 1 ); // include OPC
					EQUSTA_len = data_len + 1;
					}
				else
				{
					IsOWS = false;
					IsCTT10 = false;

		    	 	// Change verbose state to UP, if not in a call
		    	 	//
		    		if ( verb_state != VERBOSE_TRANSMISSION
		    		&&   verb_state != VERBOSE_RINGING )
		    		{
						SetVerb_State( VERBOSE_UP );
						}
					}
                }
            break;

        case FNC_PRGFNCACT:
            //
            // Function: The signal sends information concerning any pushed programmable
            // function key. A signal can contain only one key number.
            //
            // Return signal to: RELPRGFNCREQ2
            //
            //  0  8, ! 02 H FNC (Function code)
            //  1  8, ! XX H Key number
            //
            break;

        case FNC_FIXFNCACT:
            //
            // Function: The signal sends information concerning any pushed fixed function
            // key or hook state change. A signal can contain only one key code or
            // hook state change.
            //
            // Return signal to: RELFIXFNCREQ2
            //
            // D00 8, ! 05 H NBYTES
            // D01 8, ! 001/X/XXX/X OPC/X/SN/IND
            // D02 8, ! 04 H FNC (Function code)
            // D03 8, ! XX H Key code
            // D04 8, ! YY H CS
            //
            break;

        case FNC_PRGFNCREL2:
            //
            // Function: The signal is a response to the request concerning any released
            // programmable function key in the instrument. The signal can only be
            // sent once after each received request signal.
            // If no programmable function key is pushed when the request signal
            // RELPRGFNCREQ2 is received, the signal PRGFNCREL2 is
            // immediately returned.
            // If no programmable function key is pushed when the request signal
            // RELPRGFNCREQ2 is received, the signal PRGFNCREL2 will be
            // sent when the actual key is released.
            //
            // Return signal to: RELPRGFNCREQ2
            //
            // D00 8, ! 04 H NBYTES
            // D01 8, ! 001/X/XXX/X OPC/X/SN/IND
            // D02 8, ! 05 H FNC (Function code)
            // D03 8, ! YY H CS
            //
            break;

        case FNC_FIXFNCREL2:
            //
            // Function: The signal is a response to the request concerning any released
            // fixed function key in the instrument. The signal can only be sent once
            // after each received request signal.
            // If no fixed function key is pushed when the request signal
            // RELFIXFNCREQ2 is received, the signal PRGFNCREL2 is
            // immediately returned.
            // If any fixed function key is pushed when the request signal
            // RELFIXFNCREQ2 is received, the signal FIXFNCREL2 will be sent
            // when the actual key is released.
            //
            // Return signal to: RELFIXFNCREQ2
            //
            // D00 8, ! 04 H NBYTES
            // D01 8, ! 001/X/XXX/X OPC/X/SN/IND
            // D02 8, ! 06 H FNC (Function code)
            // D03 8, ! YY H
            //
            break;

        case FNC_EQULOCALTST:
            //
            // Function: The signal is sent before entering local test mode if the instrument is
            // active. The signal is also sent when the instrument has returned
            // from local test mode back to normaltraffic mode.
            // If the instrument is not active, no signal is sent before or after visiting
            // local test mode.
            //
            // Return signals to: EQUTSTREQ, Test code 147-151, 153
            //
            // D00 8, ! 05 H NBYTES
            // D01 8, ! 001/X/XXX/X OPC/X/SN/IND
            // D02 8, ! 07 H FNC (Function code)
            // D03 8, ! XX H TRAFFIC STATUS
            // D04 8, ! YY H CS
            // Detailed description:
            // D03 TRAFFIC STATUS
            // : 00 H = Instrument in normal traffic mod
            // (Return from local test mode)
            // : 01 H = Instrument in local test mode
            // (Entering local test mode)
            //
            break;

        case FNC_EXTERNUNIT:
            //
            // Function: The signal sends the signal received in the instrument from the
            // external unit, ex. a EQUSTA signal from a DBY409.
            //
            // Return signal to: None
            //
            // D00 8, ! 07-1B H NBYTES
            // D01 8, ! 001/X/XXX/X OPC/X/SN/IND
            // D02 8, ! 08 H FNC (Function code)
            // D03 8, ! XX H Extern unit address
            // D04 8, ! NNH NBYTES
            // D05 8, ! XX H Data 1
            // : :
            // D(n+4) 8, ! XX H Data n (Max 21)
            // D(n+5) 8, ! YY H CS
            //
            // Detailed description:
            // D03 EXTERN UNIT ADDRESS
            // B0-B3 : 0 - 14 = Extern unit address
            // B4-B6 : 0 = Extern unit interface
            // : 1 = Option unit interface
            // D04 NBYTES
            // Number of data bytes, including itself.
            // D05-D(n+4) DATA 1-N
            // Data received from an extern unit.
            //
            break;

        case FNC_STOPWATCHREADY:
            //
            // Function: The signal is sent to the system software when the stopwatch has
            // finished counting down to zero.
            //
            // Return signal to: None
            //
            // D00 8, ! 04 H NBYTES
            // D01 8, ! 001/X/XXX/X OPC/X/SN/IND
            // D02 8, ! 09 H FNC (Function code)
            // D03 8, ! YY H CS
            //
            break;

        case FNC_MMEFNCACT:
            break;

        case FNC_HFPARAMETERRESP:
            //
            // Function: The signal contains the transmission parameters read from the DSP
            // based handsfree circuit.
            // A specific parameter address can contain 1 to 10 data bytes.
            //
            // Return signal to: HANDSFREEPARAMETER
            //
            // D00 8, ! 07-10 H NBYTES
            // D01 8, ! 001/X/XXX/X OPC/X/SN/IND
            // D02 8, ! 0B01 H FNC (Function code)
            // D04 8, ! XX H Parameter address
            // D05 8, ! XX H Parameter data 1
            // : :
            // D(n+4) 8, ! XX H Parameter data n
            // D(n+5) 8, ! YY H CS
            //
            // Detailed description:
            // D05 PARAMETER ADDRESS
            // B0-B6 : 1 = Speech to Echo Comparator levels, 4 data bytes
            // : 2 = Switchable Attenuation settings, 4 data bytes
            // : 3 = Speech Comparator levels, 8 data bytes
            // : 4 = Speech Detector levels, 5 data bytes
            // : 5 = Speech Detector timing, 10 data bytes
            // : 6 = AGC Receive, 7 data bytes
            // : 7 = AGC Transmit, 6 data bytes
            //
            break;

        case FNC_EQUTESTRES:
            //
            // Function: The signal responds to the signal EQUTESTREQ and sends results
            // according to the requested test code.
            // If the initialisation test wasn't ok, the signal EQUTESTRES test code
            // = 127 is returned when the instrument is activated by the signal
            // EQUACT.
            //
			if ( data[ 1 ] == 0x7F ) // Test code is Local Error Status Report
			{
				if (  data[ 2 ] != 0x00 ) // Fault code
				{
					IsOWS = false;
					IsCTT10 = false;
					transmission_order = 0;
					bChannel->StopTransmission ();
					SetVerb_State( VERBOSE_FAULTY_DTS );
					}
				}
            break;

        default:
            //
            // Unknown FNC or not properly parsed.
            //
            break;
        }
	}

void ELU28_D_Channel::ParsePDU_FromPBX( unsigned char* data, int data_len )
{
    *data++; // Skip OPC
    --data_len;
    
	// Loop parsing FNCs (until there is data)
	
	// Loop will break either if there is no data (data_len <= 0), or
	// the rest of PDU should be ignored (indicated arg_len <= 0)
	//
PARSE_NEXT_FNC:

	if ( data_len <= 0 )
		return;

    int arg_len = -1;
    
    switch( *data )
    {
        case FNC_EQUACT:                
            //
            // Function: The signal activates the equipment, i.e. the instrument 
            //    changes from passive to active state. It also sets the LED indicator 
            //    and tone ringer cadences. Normally the signal EQUSTA is returned as 
            //    a response to this signal. However, if some errors has been found 
            //    in the initialisation test, the signal EQUTESTRES, Test code 127, is
            //    returned.
            //
            // Return signals: EQUSTA or EQUTESTRES, Test code 127
            //
            // D(n)    8, ! 80 H FNC (Function code)
            // D(n+1)  8, ! XX H Internal ring signal, 1:st on interval
            // D(n+2)  8, ! XX H Internal ring signal, 1:st off interval
            // D(n+3)  8, ! XX H Internal ring signal, 2:nd on interval
            // D(n+4)  8, ! XX H Internal ring signal, 2:nd off interval
            // D(n+5)  8, ! XX H External ring signal, 1:st on interval
            // D(n+6)  8, ! XX H External ring signal, 1:st off interval
            // D(n+7)  8, ! XX H External ring signal, 2:nd on interval
            // D(n+8)  8, ! XX H External ring signal, 2:nd off interval
            // D(n+9)  8, ! XX H Call back ring signal, 1:st on interval
            // D(n+10) 8, ! XX H Call back ring signal, 1:st off interval
            // D(n+11) 8, ! XX H Call back ring signal, 2:nd on interval
            // D(n+12) 8, ! XX H Call back ring signal, 2:nd off interval
            // D(n+13) 8, ! XX H LED indicator cadence 0, 1:st on interval
            // D(n+14) 8, ! XX H LED indicator cadence 0, 1:st off interval
            // D(n+15) 8, ! XX H LED indicator cadence 0, 2:nd on interval
            // D(n+16) 8, ! XX H LED indicator cadence 0, 2:nd off interval
            // D(n+16) 8, ! XX H LED indicator cadence 0, 2:nd off interval
            // D(n+17) 8, ! XX H LED indicator cadence 1, On interval
            // D(n+18) 8, ! XX H LED indicator cadence 1, Off interval
            // D(n+19) 8, ! XX H LED indicator cadence 2, On interval
            // D(n+20) 8, ! XX H LED indicator cadence 2, Off interval
            //
            // Detailed description:
            // D(n+1)-D(n+12) = 00 - FF H Number of 50 ms intervals
            // D(n+13)-D(n+20) = 00 - FF H Number of 10 ms intervals
            //
            // Note 1: Not all intervals for any cadence can be zero.
            //
            // Note 2: To a DBY409 the EQUACT signal should only contain the following bytes:
            //
            // D(n)    8, ! 80 H FNC
            // D(n+1)  8, ! XX H LED indicator cadence 0, 1:st on interval
            // D(n+2)  8, ! XX H LED indicator cadence 0, 1:st off interval
            // D(n+3)  8, ! XX H LED indicator cadence 0, 2:nd on interval
            // D(n+4)  8, ! XX H LED indicator cadence 0, 2:nd off interval
            // D(n+5)  8, ! XX H LED indicator cadence 1, On interval
            // D(n+6)  8, ! XX H LED indicator cadence 1, Off interval
            // D(n+7)  8, ! XX H LED indicator cadence 2, On interval
            // D(n+8)  8, ! XX H LED indicator cadence 2, Off interval
            //
            arg_len = 20; // = 8; // if ( DTS == DBY409 )
            break;

        case FNC_EQUSTAREQ:
            //
            // Function: The signal sends request for DTS status.
            //
            // Return signals: EQUSTA
            //
            // D(n)   8, ! 81 H FNC (Function code)
            //
            arg_len = 0;
            break;

        case FNC_CLOCKCORRECT:
            //
            // Function: The signal adjusts the local real time clock in the instrument.
            //
            // Return signals: None
            //
            // D(n)   8, ! 82 H FNC (Function code)
            // D(n+1) 8, ! XX H Type of time presentation
            // D(n+2) 8, ! XX H Clock display field
            // D(n+3) 8, ! XX H Start position within field
            // D(n+4) 8, ! XX H Hours in BCD-code
            // D(n+5) 8, ! XX H Minutes in BCD-code
            // D(n+6) 8, ! XX H Seconds in BCD-code
            //
            arg_len = 6;
            break;

        case FNC_TRANSMISSION:
            //
            // Signal: TRANSMISSION
            // Function: The signal updates the transmission.
            //
            // Return signal: None
            //
            // D(n)   8, ! 84 H FNC (Function code)
            // D(n+1) 8, ! XX H Transmission order
            //
            // Detailed description:
            // D(n+1): TRANSMISSION ORDER
            // : 01 = Handset speaking
            // : 02 = Handsfree speaking
            // : 03 = No transmission change
            // : 04 = Public address
            // : 05 = Loud speaking
            // : 06 = Headset speaking
            //
            if ( IsCTT10 )
            {
            	arg_len = -1;
            	}
            else if ( IsOWS )
            {
            	arg_len = -1;

				if ( ! transmission_order )
				{            	
					tracef( "%c: Set OWS *t", id );
					
				    transmission_order = 0x01;
					bChannel->StartTransmission ();
					SetVerb_State( VERBOSE_TRANSMISSION );
					}
            	}
            else
            {
	            arg_len = 1;
	
				if ( data[ 1 ] != 0x03 )
					transmission_order = data[ 1 ];
	
	        	if ( transmission_order )
	        	{
	        		bChannel->StartTransmission ();
	        		SetVerb_State( VERBOSE_TRANSMISSION );
	        		}
	        	else
	        	{
	        		bChannel->StopTransmission ();
	        		SetVerb_State( VERBOSE_UP );
	        		}
				}
            break;

        case FNC_CLEARDISPLAY:
            //
            // Function: The signal clears the display completely.
            //
            // Return signals: None
            //
            // D(n)  8, ! 85 H FNC (Function code)
            //
            if ( IsCTT10 )
            	arg_len = -1;
            else
            	arg_len = 0;
            break;

        case FNC_CLEARDISPLAYFIELD:
            //
            // Function: The signal clears a field in the display.
            //
            // Return signals: None
            //
            // D(n)   8, ! 86 H FNC (Function code)
            // D(n+1) 8, ! XX H Field address
            //
            // Detailed description:
            // D(n+1) FIELD ADDRESS
            // B0-B4: Row number of display field
            // B5-B7: Column number of display field
            //
            if ( IsCTT10 )
            	arg_len = -1;
            else
            	arg_len = 1;
            break;

        case FNC_INTERNRINGING:
            //
            // Function: The signal activates the internal ringing.
            //
            // Return signals: None
            //
            // D(n) 8, ! 88 H FNC (Function code)
            //
            if ( IsOWS )
            {
            	arg_len = 2 + data[ 2 ]; // FIXME
            	}
            else
            {
            	arg_len = 0;
            	SetVerb_State( VERBOSE_RINGING );
           		}
            break;

        case FNC_INTERNRINGING1LOW:
            //
            // Function: The signal activates the internal ringing 1:st interval low.
            //
            // Return signals: None
            //
            // D(n) 8, ! 89 H FNC (Function code)
            //
            if ( IsOWS )
            {
            	arg_len = 3;
            	}
           	else
           	{
            	arg_len = 0;
            	}
            break;

        case FNC_EXTERNRINGING:
            //
            // Function: The signal activates the external ringing.
            //
            // Return signals: None
            //
            // D(n) 8, ! 8A H FNC (Function code)
            //
            arg_len = 0;
            SetVerb_State( VERBOSE_RINGING );
            break;

        case FNC_EXTERNRINGING1LOW:
            //
            // Function: The signal activates the external ringing 1:st interval low
            //
            // Return signals: None
            // 
            // D(n) 8, ! 8B H FNC (Function code)
            //
            arg_len = 0;
            break;

        case FNC_CALLBACKRINGING:
            //
            // Function: The signal activates the call back ringing.
            //
            // Return signals: None
            //
            // D(n) 8, ! 8C H FNC (Function code)
            //
            if ( IsOWS )
            {
            	arg_len = 2;
            	}
            else if ( data_len >= 2 )
            {
            	arg_len = 2;

				if ( ! IsOWS )
				{
					tracef( "%c: Set DTS *r", id );
					}
					
				IsOWS = true;

				if ( ! transmission_order )
				{
					transmission_order = 0x01;
        			bChannel->StartTransmission ();
        			SetVerb_State( VERBOSE_TRANSMISSION );
        			}
            	}
            else
            {
            	arg_len = 0;
            	SetVerb_State( VERBOSE_RINGING );
            	}
            break;

        case FNC_CALLBACKRINGING1LOW:
            //
            // Function: The signal activates the call back ringing 1:st interval low.
            //
            // Return signals: None
            //
            // D(n) 8, ! 8D H FNC (Function code)
            //
            arg_len = 0;
            break;

        case FNC_STOPRINGING:
            //
            // Function: The signal stops the ringing.
            //
            // Return signals: None
            //
            // D(n) 8, ! 8E H FNC (Function code)
            //
            arg_len = 0;

            if ( transmission_order )
            	SetVerb_State( VERBOSE_TRANSMISSION );
            else
            	SetVerb_State( VERBOSE_UP );
            
            break;

        case FNC_CLEARLEDS:
            //
            // Function: The signal clears all LED indicators.
            //
            // Return signals: None
            //
            // D(n) 8, ! 8F H FNC (Function code)
            //
            arg_len = 0;
            break;

        case FNC_CLEARLED:
            //
            // Function: The signal clears one LED indicator.
            //
            // Return signals: None
            //
            // D(n)   8, ! 90 H FNC (Function code)
            // D(n+1) 8, ! XX H Indicator number
            //
            arg_len = 1;
            break;

        case FNC_SETLED:
            //
            // Function: The signal sets one LED indicator to steady light.
            //
            // Return signals: None
            //
            // D(n)   8, ! 91 H FNC (Function code)
            // D(n+1) 8, ! XX H Indicator number
            //
            arg_len = 1;
            break;

        case FNC_FLASHLEDCAD0:
            //
            // Function: The signal sets one LED indicator to flash with cadence 0.
            //
            // Return signals: None
            //
            // D(n)   8, ! 92 H FNC (Function code)
            // D(n+1) 8, ! XX H Indicator number
            //
            arg_len = 1;
            break;

        case FNC_FLASHLEDCAD1:
            //
            // Function: The signal sets one LED indicator to flash with cadence 1.
            //
            // Return signals: None
            //
            // D(n)   8, ! 93 H FNC (Function code)
            // D(n+1) 8, ! XX H Indicator number
            //
            arg_len = 1;
            break;

        case FNC_FLASHLEDCAD2:
            //
            // Function: The signal sets one LED indicator to flash with cadence 2.
            //
            // Return signals: None
            //
            // D(n)   8, ! 94 H FNC (Function code)
            // D(n+1) 8, ! XX H Indicator number
            //
            arg_len = 1;
            break;

        case FNC_WRITEDISPLAYFIELD:
            //
            // Function: The signal writes 10 characters to a display field.
            //
            // Return signals: None
            //
            // D(n)   8, ! 95 H FNC (Function code)
            // D(n+1) 8, ! XX H Display field address
            // D(n+2)    ! XX H
            //
            // D(n+11)   ! XX H 10 Characters to the display
            //
            // Detailed description:
            // D(n+1) DISPLAY FIELD ADDRESS
            // B0-B4: Row number of display field
            // B5-B7: Column number of display field
            //
            arg_len = 11;
            break;

        case FNC_RNGCHRUPDATE:
            //
            // Function: The signal updates the chosen tone ringer character in the
            // instrument.
            //
            // Return signals: None
            //
            // D(n) 8, ! 96 H FNC (Function code)
            // D(n+1) 8, ! XX H Tone ringer character order
            //
            // Detailed description:
            // D(n+1) TONE RINGER CHARACTER ORDER
            // B0-B7: One value form 00 to 09 in Hex code, meaning
            // that ten different ringing tone characters can be
            // chosen in the instrument.
            //
            // Note 1: Default value in the instrument in 05.
            //
            arg_len = 1;
            break;

        case FNC_STOPWATCH:
            //
            // Function: The signal controls a local stopwatch function in the 
            // instrument according to the description below.
            //
            // Return signals: None
            //
            // D(n) 8, ! 97 H FNC (Function code)
            // D(n+1) 8, ! XX H Stopwatch display field
            // D(n+2) 8, ! XX H Start position within field
            // D(n+3) 8, ! XX H Stopwatch order
            // D(n+4) 8, ! XX H Minutes in BCD-code
            // D(n+5) 8, ! XX H Seconds in BCD-code, see note 2
            //
            // Detailed description:
            // D(n+1) STOPWATCH DISPLAY FIELD
            //    B0-B4: Row number of display field
            //    B5-B7: Column number of display field
            // D(n+2) START POSITION WITHIN FIELD
            //    B0-B3: Start position of stopwatch in display field
            //    B4-B7: 0
            // D(n+3) STOPWATCH ORDER
            //   B0-B1 = 0 : Reset to zero
            //         = 1 : Stop counting
            //         = 2 : Start counting
            //         = 3 : Don't affect the counting
            //      B2 = 0 : Remove stopwatch from display
            //         = 1 : Present stopwatch on display
            //      B3 = 0 : Increment counting
            //         = 1 : Decrement counting
            //      B4 = 0 : Don't update stopwatch time
            //         = 1 : Update stopwatch with new time
            //
            // Note 1: The bit 3, 4 in Stopwatch order and minutes and seconds don't 
            // exist in DBC600 telephones.
            //
            // Note 2: If stopwatch order bit 4 = 0, when the minutes and seconds 
            // must not be sent.
            //
            arg_len = 5;
            break;

        case FNC_DISSPECCHR:
            //
            // Function: The signal defines a special character to the display. One special
            // character per signal can be defined formatted as a 8x5 dot matrix.
            // Maximum 8 different characters can be defined for one instrument.
            //
            // Return signals: None
            //
            // D(n)   8, ! 98 H FNC (Function code)
            // D(n+1) 8, ! ZZ H Character code
            // D(n+2) 8, ! XX H Data in dot row 1
            // D(n+3) 8, ! XX H Data in dot row 2
            // D(n+4) 8, ! XX H Data in dot row 3
            // D(n+5) 8, ! XX H Data in dot row 4
            // D(n+6) 8, ! XX H Data in dot row 5
            // D(n+7) 8, ! XX H Data in dot row 6
            // D(n+8) 8, ! XX H Data in dot row 7
            // D(n+9) 8, ! XX H Data in dot row 8
            //
            // Detailed description:
            // D(n+1) CHARACTER CODE
            //    B0-B7: 00 - 07 H, which means that 8 different characters can be defined.
            // D(n+2)-D(n+9) DATA IN DOT ROW 1 TO 8
            //    B0-B4: Dot definition in one row. "1" means active dot.
            //    B5-B7: 0
            //
            arg_len = 9;
            break;

        case FNC_FLASHDISPLAYCHR:
            // 
            // Function: The signal activates blinking character string in thedisplay, max 10
            // characters.
            //
            // Return signals: None
            //
            // D(n)   8, ! 99 H FNC (Function code)
            // D(n+1) 8, ! XX H Display field address
            // D(n+2)    ! NY H Start position number of characters
            // D(n+3)    ! XX1 H Character to blink
            //
            // D(n+X)    ! XXN H
            //
            // Detailed description:
            // D(n+1) DISPLAY FIELD ADDRESS
            //    B0-B4: Row number of display field
            //    B5-B7: Column number of display field
            // D(n+2) START POSITION, NUMBER OF CHARACTERS
            //    B0-B3: Start position in field
            //    B4-B7: Number of characters, (1-10)
            // D(n+3)..D(n+X) CHARACTERS TO BLINK
            //    The number must be the same as in "number of characters" above,
            //    max 10 characters.
            //
            arg_len = 2 + ( data[ 2 ] >> 4 ) & 0x0F;
            break;

        case FNC_ACTCLOCK:
            // 
            // Function: The signal activates the real time clock in the display.
            //
            // Return signals: None
            // 
            // D(n)   8, ! 9A H FNC (Function code)
            // D(n+1) 8, ! XX H Clock order
            //
            // Detailed description:
            // D(n+1) CLOCK ORDER
            // B0-B6 : 1 = Clock in display0
            //    B7 : 1 = Clock in display
            //       ; 0 = No clock in display
            //
            if ( IsCTT10 )
            	arg_len = -1;
            else
            	arg_len = 1;
            break;

        case FNC_RELPRGFNCREQ2:
            // 
            // Function: The signal requests information concerning released programmable
            // function key from the instrument. If no programmable function key is
            // pushed when this signal is received, the signal PRGFNCREL2 is
            // returned back at once. If a programmable function key is pushed
            // when this signal is received, the signal PRGFNCREL2 is returned
            // back when the actual key become released. Just one return signal
            // PRGFNCREL2 is associated with any received signal
            // RELPRGFNCREQ2.
            //
            // Return signals: PRGFNCREL2
            //
            // D(n) 8, ! 9B H FNC (Function code)
            //
            arg_len = 0;
            break;

        case FNC_RELFIXFNCREQ2:
            // 
            // Function: The signal requests information concerning released fix function key
            // from the instrument. If no fix function key is pushed when this signal
            // is received, the signal FIXFNCREL2 is returned back at once. If a fix
            // function key is pushed when this signal is received, the signal
            // FIXFNCREL2 is returned back when the actual key become
            // released.
            // Just one return signal FIXFNCREL2 is associated with any received
            // signal RELFIXFNCREQ2.
            //
            // Return signals: FIXFNCREL2
            //
            // D(n) 8, ! 9C H FNC (Function code)
            //
            arg_len = 0;
            break;

        case FNC_ACTCURSOR:
            // 
            // Function: The signal activates the blinking cursor in the display.
            //
            // Return signals: None
            //
            // D(n)   8, ! 9D H FNC (Function code)
            // D(n+1) 8, ! XX H Cursor display address
            // D(n+2) 8, ! XX H Cursor position within field
            //
            // Detailed description:
            // D(n+1) CURSOR DISPLAY ADDRESS
            //    B0-B4: Row number of display field
            //    B5-B7: Column number of display field
            // D(n+1) CURSOR POSITION WITHIN FIELD
            //    B0-B3: Cursor position within field, (0-9)
            //    B4-B7: 0
            //
            arg_len = 2;
            break;

        case FNC_CLEARCURSOR:
            //
            // Function: The signal clears the blinking cursor from the display.
            //
            // Return signals: None
            //
            // D(n) 8, ! 9E H FNC (Function code)
            //
            arg_len = 0;
            break;

        case FNC_FIXFLASHCHR:
            //
            // Function: The signal changes the blinking character string to permanent
            // presentation.
            //
            // Return signals: None
            // 
            // D(n) 8, ! 9F H FNC (Function code)
            //
            arg_len = 0;
            break;

		case FNC_A1:
			//
			// Function: Unknown / Seen on CTT10 / Appearing acknowledging successfull login.
			//
			arg_len = -1;
			
			IsCTT10 = true;
			
			if ( ! transmission_order )
			{
				transmission_order = 0x01;
    			bChannel->StartTransmission ();
    			SetVerb_State( VERBOSE_TRANSMISSION );
    			}
			break;
			
		case FNC_A2:
		case FNC_A3:
		case FNC_A4:
			//
			// Function: Unknown / Seen on CTT10 / 0xA2 = Login failed; 0xA3 = Logout; 0xA4 = ?
			//
			arg_len = -1;
			
			if ( IsCTT10 )
			{
            	transmission_order = 0;
            	bChannel->StopTransmission ();
    			SetVerb_State( VERBOSE_UP );
    			}
			break;
			
        case FNC_RNGLVLUPDATE:
            //
            // Function: The signal updates the tone ringer level and controls if ringing shall
            // be with increasing level. It also decides if a test for four periods shall
            // be made.
            //
            // Return signals: None
            //
            // D(n) 8, ! A7 H FNC (Function code)
            // D(n+1) 8, ! XX H Tone ringer level order
            //
            // Detailed description:
            // D(n+1) TONE RINGER LEVEL ORDER
            //  B0-B7: Tone ringer level, 0dB-(-27dB) step -3dB
            //       : 0 = 0dB (Max)
            //       : 1 = -3dB
            //       : 9 = -27dB (Min)
            //     B4: 0 = Normal ringing
            //       : 1 = Increasing tone ringer level
            //     B5: 0 = No activation of tone ringer
            //       : 1 = Activation of tone ringer during 4 periods.
            //
            // Note 1: In the DBC199 telephone the tone ringer level (B0-B3) can only have two values,
            // 0 = Maximum level and 9 = Attenuated level.
            //
            // Note 2: In the DBC199 telephone the increasing tone ringer level (B4 = 1) means that the two
            // first periods are attenuated and the remaining are with maximum level.
            //
            arg_len = 1;
            break;

        case FNC_TRANSLVLUPDATE:
            //
            // Function: The signal updates the transmission levels in the DTS. The signal
            // also tells the instrument what the PBX has D3 comparability. Before
            // this signal has been received the D3 instrument acts like a DBC600.
            //
            // Return signal: None
            //
            // D(n) 8, ! A8 H FNC (Function code)
            // D(n+1) 8, ! XX H Loudspeaker level
            // D(n+2) 8, ! XX H Earpiece level
            //
            // Detailed description:
            // D(n+1) LOUDSPEAKER LEVEL (RLR)
            // B0-B7 : Loudspeaker level, -6dB - +14dB step 2dB
            // : 00 = -6dB (23dBr, Max)
            // : 01 = -4dB (21dBr)
            // : 0A = 14dB (3dBr, Min)
            // D(n+2) EARPIECE LEVEL (RLR)
            // B0-B7 : Earpiece level, -5dB - +10dB step 1dB
            // : 00 = -5dB (-13dBr, Max)
            // : 01 = -4dB (-14dBr)
            // : 0F = 10dB (-28dBr, Min)
            //
            arg_len = 2;
            break;

        case FNC_IO_SETUP:
            //
            // Function: The signal updates the I/O optional board.
            //
            // Return signals: None
            //
            // D(n) 8, ! A9 H FNC (Function code)
            // D(n+1) 8, ! XX H I/O setup order 1
            // D(n+2) 8, ! XX H I/O setup order 2
            //
            // Detailed description:
            // D(n+1) I/O SETUP ORDER 1
            //   : 0 =Extern function, Updated by PBX
            //   : 1 =Extern function, Internally updated
            //   : 2 =Extra handset mode update
            //   : 3 =Impaired handset level
            //   : 4 =PC sound modes
            //   : 5 =Option unit mute
            // D(n+2) I/O SETUP ORDER 2
            // D(n+1): 0 = EXTERN FUNCTION, UPDATED BY PBX:
            // D(n+2) : 0 =Extern function, I/O off
            //   : 1 =Extern function, I/O on
            // D(n+1): 1 = EXTERN FUNCTION, INTERNALLY UPDATED:
            // D(n+2) : 0 =Extern function, Tone ringer
            //   : 1 =Extern function, Extern busy indicator
            // D(n+1): 2 = EXTRA HANDSET MODE UPDATE:
            //   B0 : 0 =Handset microphone off
            //      : 1 =Handset microphone on
            //   B1 : 0 =Extra handset microphone off
            //      : 1 =Extra handset microphone on
            // D(n+1): 3 = IMPAIRED HANDSET LEVEL:
            // D(n+2) : 0 =Impaired level off
            //      : 1 =Impaired level on
            //
            arg_len = 2;
            break;

        case FNC_FLASHDISPLAYCHR2:
            //
            // Function: The signal activates blinking character string in the display, max 20
            // characters.
            //
            // Return signals: None
            //
            // D(n) 8, ! AA H FNC (Function code)
            // D(n+1) 8, ! XX H Display field address
            // D(n+2) 8, ! XX H Start position
            // D(n+3) 8, ! NN H Number of characters
            // D(n+4) 8, ! XX H Character to blink
            // ...
            // D(n+X) 8, ! XXN H
            //
            // Detailed description:
            // D(n+1) DISPLAY FIELD ADDRESS
            //   B0-B4: Row number of display field
            //   B5-B7: Column number of display field
            // D(n+2) START POSITION
            //   B0-B3: Start position in field, (0-9)
            //   B4-B7: 0
            // D(n+3) NUMBER OF CHARACTERS
            //   B0-B7: Number of characters, (1-20)
            // D(n+4) CHARACTERS STRING
            //   The number must be the same as in "number of characters" above,
            //   max 20 characters.
            //
            arg_len = 2 + data[ 3 ];
            break;

        case FNC_TRANSCODEUPDATE:
            //
            // Function: The signal updates transmissions coding principle and markets
            // adaptations for sidetone and transmit level in the DTS.
            //
            // Return signals: None
            //
            // D(n) 8, ! AB H FNC (Function code)
            // D(n+1) 8, ! XX H PCM code order
            // D(n+2) 8, ! XX H Sidetone level
            // D(n+3) 8, ! XX H Transmit level
            //
            // Detailed description:
            // D(n+1) PCM CODE ORDER
            //   : 0 = my-law
            //   : 1 = A-law
            // D(n+2) SIDETONE LEVEL (STMR = SLR - S - SK)
            //   0-B7 : Sidetone level, S=-8dB-(-23dB) step -1dB
            //   S : 00 = -8dB (Max.)
            //   S : 01 = -9dB
            //   S : 0F = -23dB (Min.)
            // D(n+3) TRANSMIT LEVEL (SLR = TK - T)
            //   B0-B7 : Transmit level, T=0dB-15dB step 1dB
            //   T : 00 = 0dB (Min.)
            //   T : 01 = 1dB
            //   T : 0F = 15dB (Max.)
            //
            // Note: The constants SK and TK equipment type dependent:
            // DBC200 : SK = 4.5, TK = 15.
            //
            arg_len = 3;

        	bChannel->Initialize( this, /* ALaw= */ data[ 1 ] != 0 );

            break;

        case FNC_EXTERNUNIT1UPDATE:
            // 
            // Function: The signal sends the data in the signal unchanged to external unit or
            // option un
            //
            // Return signals: None
            //
            // D(n) 8, ! AC-AD H FNC (Function code)
            // D(n+1) 8, ! XX H Extern unit address
            // D(n+2) 8, ,! 02-17 H NBYTES
            // D(m) 8, ! XX H Data 1
            // ...
            // D(m+N) Data n (max 22)
            //
            // Detailed description:
            // D(n) FUNCTION CODE
            //   : AC H = Extern unit 1 (if old format)
            //   : AD H = Extern unit 2 (if old format)
            // D(n+1) EXTERN UNIT ADDRESS
            //   0- B0-B3 : 0 - 14 = Extern unit address
            //   : 15 = Extern unit broadcast
            //   B4-B6 : 0 = Extern unit interface
            //   : 1 = Option unit interface
            //   B7 : 0 = Old format (D(n+1) = NBYTES)
            //   : 1 = Enhanced format
            // D(n+2) NBYTES
            //   Number of data bytes, including itself.
            // D(m)-D(m+N) DATA 1-N
            //   Data to be sent to extern unit 1.
            //
            // Note: The OPC/X/SN/IND byte shall not be included in the DATA 1-N field in signals to an
            // extern unit.
            //
            arg_len = 2 + data[ 2 ] - 1;
            break;

        case FNC_EXTERNUNIT2UPDATE:
            // The same as previous; but UNIT2
            arg_len = 2 + data[ 2 ] - 1;
            break;

        case FNC_WRITEDISPLAYCHR:
            //
            // Function: The signal writes character string in the display, max 40 characters.
            //
            // Return signals: None
            //
            // D(n) 8, ! AE H FNC (Function code)
            // D(n+1) 8, ! XX H Display field address
            // D(n+2) 8, ! XX H Start position
            // D(n+3) 8, ! XX H Number of characters
            // D(n+4) 8, ! XX1 H Character string
            // ...
            // D(n+X) ! XXN H
            //
            // Detailed description:
            // D(n+1) DISPLAY FIELD ADDRESS
            //   B0-B4: Row number of display field
            //   B5-B7: Column number of display field
            // D(n+2)S TART POSITION
            //   B0-B3: Start position in field, (0-9)
            //   B4-B7: 0
            // D(n+3) NUMBER OF CHARACTERS
            //   B0-B7: Number of characters, (1-40)
            // D(n+4) CHARACTERS STRING
            //   The number must be the same as in "number of characters" above,
            //   max 40 characters.
            arg_len = 3 + data[ 3 ];
            break;

        case FNC_ACTDISPLAYLEVEL:
            //
            // Function: The signal activates the level bar in the display.
            //
            // Return signals: None
            //
            // D(n) 8, ! AF H FNC (Function code)
            // D(n+1) 8, ! XX H Display level bar address
            // D(n+2) 8, ! XX H Display level bar start position within field
            // D(n+3) 8, ! XX H Length of level bar (1-40 char)
            // D(n+4) 8, ! XX H Level of bar (0-40 char)
            //
            // Detailed description:
            // D(n+1) DISPLAY LEVEL BAR ADDRESS
            //   B0-B4: Row number of display field
            //   B5-B7: Column number of display field
            // D(n+2) DISPLAY LEVEL BAR START POSITION WITHIN FIELD
            //   B0-B3: Display level bar position within field, (0-9)
            //   B4-B7: 0
            //
            arg_len = 4;
            break;

        case FNC_ANTICLIPPINGUPDATE:
            // Function: The signal updates the transmission Anti clipping to be on or off and
            // the Anti clipping threshold.
            //
            // Return signal: None
            //
            // D(n) 8, ! B0 H FNC (Function code)
            // D(n+1) 8, ! XX H Anti clipping on/off
            // D(n+2) 8, ! XX H Anti clipping threshold
            //
            // Detailed description:
            // D(n+2) D(n+1)ANTI CLIPPING ON/OFF
            //   : 00 = Anti clipping off
            //   : 01 = Anti clipping on
            // D(n+2) ANTI CLIPPING THRESHOLD
            //   B0-B7 : 00 = -15dm0 Threshold
            //   : 01 = -13dm0 Threshold
            //   : 02 = -9dm0 Threshold
            //   : 03 = -7dm0 Threshold
            //
            arg_len = 2;
            break;

        case FNC_EXTRARNGUPDATE:
            //
            // Function: The signal updates the 3:rd tone ringer cadences for Intern, Extern
            // and Call-back and all cadences for extra tone ringing.
            //
            // Return signals: EQUSTA or EQUTESTRES, Test code 127
            //
            // D(n) 8, ! B1 H FNC (Function code)
            // D(n+1) 8, ! XX H Internal ring signal, 3:rd on interval
            // D(n+2) 8, ! XX H Internal ring signal, 3:rd off interval
            // D(n+3) 8, ! XX H External ring signal, 3:rd on interval
            // D(n+4) 8, ! XX H External ring signal, 3:rd off interval
            // D(n+5) 8, ! XX H Call back ring signal, 3:rd on interval
            // D(n+6) 8, ! XX H Call back ring signal, 3:rd off interval
            // D(n+7) 8, ! XX H Extra ring signal, 1:st on interval
            // D(n+8) 8, ! XX H Extra ring signal, 1:st off interval
            // D(n+9) 8, ! XX H Extra ring signal, 2:nd on interval
            // D(n+10) 8, ! XX H Extra ring signal, 2:nd off interval
            // D(n+11) 8, ! XX H Extra ring signal, 3:rd on interval
            // D(n+12) 8, ! XX H Extra ring signal, 3:rd off interval
            //
            // Detailed description:
            // D(n+1)-D(n+12) = 00 - FF H Number of 50 ms intervals
            //
            // Note: In DBC200 telephones the Internal and External 3:rd interval is not used.
            //
            arg_len = 12;
            break;

        case FNC_EXTRARINGING:
            //
            // Function: The signal activates the extra ringing.
            //
            // Return signals: None
            //
            // D(n) 8, ! B2 H FNC (Function code)
            //
            arg_len = 0;
            SetVerb_State( VERBOSE_RINGING );
            break;

        case FNC_EXTRARINGING1LOW:
            //
            // Function: The signal activates the extra ringing 1:st interval low.
            //
            // Return signals: None
            //
            // D(n) 8, ! B3 H FNC (Function code)
            //
            arg_len = 0;
            break;

        case FNC_HEADSETUPDATE:
            //
            // Function: The signal defines which programmable key shall be used as a
            // headset ON/OFF key.
            // When local controlled headset mode the headset key will not be sent
            // to SSW.
            // The associated LED is locally controlled after this signal.
            //
            // Return signals: None
            //
            // D(n) 8, ! B4 H FNC (Function code)
            // D(n+1) 8, ! XX H Headset mode
            // D(n+2) 8, ! XX H Headset key number (0-255)
            //
            // Detailed description:
            // D(n+1) HEADSET MODE
            // : 0 = Normal handset mode
            // : 1 = Local controlled headset mode
            // : 2 = SSW controlled headset mode passive
            // : 3 = SSW controlled headset mode active
            // D(n+2) HEADSET KEY NUMBER
            // : 0E H = Default headset key code
            // : FF H = No headset key defined
            //
            arg_len = 2;
            break;

        case FNC_HANDSFREEPARAMETER:
            //
            // Function: The signal updates the transmission parameters in the DSP based
            // handsfree circuit. A specific parameter address can contain 1 to 10
            // data bytes.
            //
            // Return signals: None or HFPARAMETERRESP
            //
            // D(n) 8, ! B5 HFNC (Function code)
            // D(n+1) 8, ! XX H Number of parameter address
            // D(n+2) 8, ! XX H Parameter address
            // D(n+3) ! XX H Parameter address
            // : :
            // D(n+k) ! XX H Parameter address
            //
            // Detailed description:
            // D(n+2) PARAMETER ADDRESS
            //    B7 : 0 = Write parameters
            //    : 1 = Read parameters
            //    B0-B6 : 1 = Speech to Echo Comparator levels, 4 data bytes
            //    : 2 = Switchable Attenuation settings, 4 data bytes
            //    : 3 = Speech Comparator levels, 8 data bytes
            //    : 4 = Speech Detector levels, 5 data bytes
            //    : 5 = Speech Detector timing, 10 data bytes
            //    : 6 = AGC Receive, 7 data bytes
            //    : 7 = AGC Transmit, 6 data bytes
            //
            // D(n+2) = SPEECH TO ECHO COMPARATOR LEVELS
            // D(n+3) : XX H Gain of acoustic echo, -48 to +48 dB step 0.38dB
            // D(n+4) : XX H Gain of line echo, -48 to +48 dB step 0.38dB
            // D(n+5) : XX H Echo time, 0 to 1020 ms step 4ms
            // D(n+6) : XX H Echo time, 0 to 1020 ms step 4ms
            //
            // D(n+2) = SWITCHABLE ATTENUATION SETTING
            // D(n+3) : XX H Attenuation if speech, 0 to 95 dB step 0.5dB
            // D(n+4) : XX H Wait time, 16ms to 4s step 16ms
            // D(n+5) : XX H Decay speed, 0.6 to 680 ms/dB step 2.66ms/dB
            // D(n+6) : XX H Switching time, 0.005 to 10 ms/dB
            //
            // D(n+2) = SPEECH COMPARATOR LEVELS
            // D(n+3) : XX H Reserve when speech, 0 to 48 dB step 0.19dB
            // D(n+4) : XX H Peak decrement when speech, 0.16 to 42 ms/dB
            // D(n+5) : XX H Reserve when noise, 0 to 48 dB step 0.19dB
            // D(n+6) : XX H Peak decrement when noise, 0.16 to 42 ms/dB
            // D(n+7) : XX H Reserve when speech, 0 to 48 dB step 0.19dB
            // D(n+8) : XX H Peak decrement when speech, 0.16 to 42 ms/dB
            // D(n+9) : XX H Reserve when noise, 0 to 48 dB step 0.19dB
            // D(n+10 : XX H Peak decrement when noise, 0.16 to 42 ms/dB
            //
            // D(n+2) = SPEECH DETECTOR LEVELS
            // D(n+3) : XX H Limitation of amplifier, -36 to -78 dB step 6.02dB
            // D(n+4) : XX H Level offset up to noise, 0 to 50 dB step 0.38dB
            // D(n+5) : XX H Level offset up to noise, 0 to 50 dB step 0.38dB
            // D(n+6) : XX H Limitation of LP2, 0 to 95 dB step 0.38
            // D(n+7) : XX H Limitation of LP2, 0 to 95 dB step 0.38
            //
            // D(n+2) = SPEECH DETECTOR TIMING
            // D(n+3) : XX H Time constant LP1, 1 to 512 ms
            // D(n+4) : XX H Time constant LP1, 1 to 512 ms
            // D(n+5) : XX H Time constant PD (signal), 1 to 512 ms
            // D(n+6) : XX H Time constant PD (noise), 1 to 512 ms
            // D(n+7) : XX H Time constant LP2 (signal), 4 to 2000 s
            // D(n+8) : XX H Time constant LP2 (noise), 1 to 512 ms
            // D(n+9) : XX H Time constant PD (signal), 1 to 512 ms
            // D(n+10 : XX H Time constant PD (noise), 1 to 512 ms
            // D(n+11) : XX H Time constant LP2 (signal), 4 to 2000 s
            // D(n+12) : XX H Time constant LP2 (noise), 1 to 512 ms
            //
            // D(n+2) = AGC RECEIVE
            // D(n+3) : XX H Loudspeaker gain, -12 to 12 dB
            // D(n+4) : XX H Compare level, 0 to -73 dB
            // D(n+5) : XX H Attenuation of control, 0 to -47 dB
            // D(n+6) : XX H Gain range of control, 0 to 18 dB
            // D(n+7) : XX H Setting time for higher levels, 1 to 340 ms/dB
            // D(n+8) : XX H Setting time for lower levels, 1 to 2700 ms/dB
            // D(n+9) : XX H Threshold for AGC-reduction, 0 to -95 dB
            //
            // D(n+2) = AGC TRANSMIT
            // D(n+3) : XX H Loudspeaker gain, -12 to 12 dB
            // D(n+4) : XX H Compare level, 0 to -73 dB
            // D(n+5) : XX H Gain range of control, 0 to 18 dB
            // D(n+6) : XX H Setting time for higher levels, 1 to 340 ms/dB
            // D(n+7) : XX H Setting time for lower levels, 1 to 2700 ms/dB
            // D(n+8) : XX H Threshold for AGC-reduction, 0 to -95 dB
            arg_len = -1; // Ignore the rest of PDU. FIXME
            break;

		case FNC_EQUSTAD4REQ:
			//
			// Function: The signal request for D4 information. If D4 terminal received this
			// signal, it will answer it with EQUSTA. But not the D3 terminals
            //
			// Return signals: EQUSTA (d4 INFORMATION )
			//
			// D(n) 8, ! D0 H FNC (Function code)
			// D(n+1) 8, ! XX H Exchange Type
			//
			// Detailed description:
			// D(n+1) Exchange Type
			// B0-B7: 0 = MD system
			//        1 = BP system
			//        2 = MDE system		
			//   
			SetVerb_State( VERBOSE_UP );
			
            arg_len = 1;
            break;

		case FNC_CLEARDISPLAYGR:
			// 
			// Function: The signal clear the display area defined by signal. clears all or part
			// of a graphic display
            //
			// Return signals: None
			// 
			// D(n) 8, ! D1 H FNC (Function code)
			// D(n+1) 16, ! XXXX H Coordinate x (start pixel) note: LSB first
			// D(n+3) 8, ! XX H Coordinate y (start pixel)
			// D(n+4) 16, ! XXXX H Coordinate x (end pixel) note: LSB first
			// D(n+6) 8, ! XX H Coordinate y (end pixel)
			// 
            arg_len = -1;
            break;

		case FNC_WRITEDISPLAYGR:
			// 
			// Function: The signal is going to write a character string in the graphic display.
			// The character string data not used will be filled with FF. Coordinate
			// (FFFF,FF) has special meaning: Write the string after the last string.
            //
			// Return signals: None
			//
			// D(n) 8, ! D2 H FNC (Function code)
			// D(n+1) 16, ! XXXX H Coordinate x (start pixel) note: LSB first
			// D(n+3) 8, ! XX H Coordinate y (start pixel)
			// D(n+4) 8, ! XX H Character description
			// D(n+5) 8, ! XX H Length of String
			// D(n+6) 8, ! XX1 H Character string
			// : :
			// D(n+X) : ! XXN H
			//
			// Detailed description:
			//
			// D(n+4) CHARACTER DESCRIPTION
			// B0-B4 = 1 : Small fonts in D4 mode
			//       = 2 : Medium fonts in D4 mode
			//       = 3 : Large fonts in D4 mode
			//       = 14 : DBC222, 223 fonts in D3 mode
			//       = 15 : DBC224,DBC225 fonts in D3 mode
			// B5-B6 = 0 : 0% of full colour
			//       = 1 : 30% of full colour
			//       = 2 : 60% of full colour
			//       = 3 : 100% of full colour
			// B7    = 0 : No blinking
			//       = 1 : Blinking		
			// 
            arg_len = -1; // Ignore the rest of PDU. FIXME
            break;

		case FNC_STOPWATCHGR:
			//
			// Function: The signal clear the display area defined by signal. clears all or part
			// of a graphic display
            //
			// Return signals: None
			//
			// D(n) 8, ! D3 H FNC (Function code)
			// D(n+1) 16, ! XXXX H Coordinate x (start pixel) note: LSB first
			// D(n+3) 8, ! XX H Coordinate y (start pixel)
			// D(n+4) 8, ! XX H Stopwatch order
			// D(n+5) 8, ! XX H Character description
			//
			// Detailed description:
			//
			// D(n+4) STOPWATCH ORDER
			// B0-B1 = 0 : Reset to zero
			//       = 1 : Stop counting
			//       = 2 : Start counting
			//       = 3 : Don’t affect the counting
			// B2    = 0 : Remove stopwatch from display
			//       = 1 : Present stopwatch on display
			// B3-B7 = 0 : Not used
			//
			// D(n+5) CHARACTER DESCRIPTION
			// B0-B4 = 1 : Small fonts in D4 mode
			//       = 2 : Medium fonts in D4 mode
			//       = 3 : Large fonts in D4 mode
			//       = 14 : DBC222, 223 fonts in D3 mode
			//       = 15 : DBC224,DBC225 fonts in D3 mode
			// B5-B6 = 0 : 0% of full colour
			//       = 1 : 30% of full colour
			//       = 2 : 60% of full colour
			//       = 3 : 100% of full colour
			// B7 : Not used
			//
			arg_len = -1;
			break;
			
		case FNC_CLOCKORGR:
			//
			// Function: The signal adjusts the local real time clock in the instrument.
            //
			// Return signals: None
			//
			// D(n) 8, ! D4 H FNC (Function code)
			// D(n+1) 8, ! XX H Type of time presentation
			// D(n+2) 16, ! XXXX H Coordinate x (start pixel) note: LSB first
			// D(n+4) 8, ! XX H Coordinate y (start pixel)
			// D(n+5) 8, ! XX H Hours in BCD-code
			// D(n+6) 8, ! XX H Minutes in BCD-code
			// D(n+7) 8, ! XX H Seconds in BCD-code
			// D(n+8) 8, ! XX H Character description
			//
			// Detailed description:
			//
			// D(n+1) TYPE OF TIME PRESENTATION
			// B0-B7 = 0 : 24-hour presentation
			// = 1 : 12-hour presentation
			//
			// D(n+5) HOURS IN BCD-CODE
			// MINUTES IN BCD-CODE
			// SECONDS IN BCD-CODE
			// B0-B3 Least significant figure in BCD code
			// B4-B7 Most significant figure in BCD code
			//
			// D(n+8) CHARACTER DESCRIPTION
			// B0-B4 = 1 : Small fonts in D4 mode
			//       = 2 : Medium fonts in D4 mode
			//       = 3 : Large fonts in D4 mode
			//       = 14 : DBC222, 223 fonts in D3 mode
			//       = 15 : DBC224,DBC225 fonts in D3 mode
			// B5-B6 = 0 : 0% of full colour
			//       = 1 : 30% of full colour
			//       = 2 : 60% of full colour
			//       = 3 : 100% of full colour
			// 
            arg_len = -1; // Ignore the rest of PDU. FIXME
            break;

		case FNC_ACTCURSORGR:
			//
			// Function: This function code is equivalent to the existing ACTIVATE BLINKING
			// CURSOR. The signal activates the blinking cursor on the display.
			// Currently, the field and position where the cursor should be displayed
			// are specified by the EXCHANGE and so will be the starting pixel.
			// The only cursor management which is handled by the firmware is to
			// remove the cursor when a new one is displayed. Coordinate
			// (FFFF,FF) has special meaning: Put the corsor after the last string.
            //
			// Return signals: None
			//
			// D(n) 8, ! D5 H FNC (Function code)
			// D(n+1) 16, ! XXXX H Coordinate x (start pixel) note: LSB first
			// D(n+3) 8, ! XX H Coordinate y (start pixel)
			// D(n+4) 8, ! XX H ASCII code for cursor (Not Used)
			// D(n+5) 8, ! XX H Character description
			//
			// Detailed description:
			//
			// D(n+5) CHARACTER DESCRIPTION
			// B0-B4 = 1 : Small fonts in D4 mode
			//       = 2 : Medium fonts in D4 mode
			//       = 3 : Large fonts in D4 mode
			//       = 14 : DBC222, 223 fonts in D3 mode
			//       = 15 : DBC224,DBC225 fonts in D3 mode
			// B5-B6 = 0 : 0% of full colour
			//       = 1 : 30% of full colour
			//       = 2 : 60% of full colour
			//       = 3 : 100% of full colour
			// B7 : Not used
			//		
			arg_len = -1; // Ignore the rest of PDU. FIXME
			break;

		case FNC_HWICONSGR:
			//
			// Function: This signal controls the local SHOWICONS function in the
			// instrument according to the description below.
            //
			// Return signals: None
			//
			// D(n) 8, ! D6 H FNC (Function code)
			// D(n+1) 16, ! XXXX H Coordinate x (start pixel) note: LSB first
			// D(n+3) 8, ! XX H Coordinate y (start pixel)
			// D(n+4) 8, ! XX H SHOWICONS order
			//
			// Detailed description:
			//
			// D(n+4) SHOWICONS ORDER
			// B0    = 0 Remove status icons from display
			//       = 1 Present status icons on display
			// B1-B7 = 0 Not used
			//
			arg_len = -1; // Ignore the rest of PDU. FIXME
			break;

		case FNC_DRAWGR:
			//
			// Function: The signal displays a line/rectangle from start pixel to end pixel
            //
			// Return signals: None
			//
			// D(n) 8, ! D7 H FNC (Function code)
			// D(n+1) 8, ! XX H Draw description
			// D(n+2) 16, ! XXXX H Coordinate x (start pixel) note: LSB first
			// D(n+4) 8, ! XX H Coordinate y (start pixel)
			// D(n+5) 8, ! XX H Extra info (not used)
			// D(n+6) 16, ! XX H Coordinate x (end pixel)
			// D(n+8) 8, ! XX H Coordinate y (end pixel)
			// D(n+9) 8, ! XX H Character description
			//
			// Detailed description:
			//
			// D(n+1) DRAW DESCRIPTION
			// B0-B3 = 1 : horizontal line
			//       = 2 : vertical line
			//       = 3 : rectangle
			//
			// D(n+9) CHARACTER DESCRIPTION
			// B0-B4 = 1 : 1 pixel thick
			//       = 2 : 2 pixels thick
			//       = 3 : 3 pixels thick
			// B5-B6 = 0 : 0% of full colour
			//       = 1 : 30% of full colour
			//       = 2 : 60% of full colour
			//       = 3 : 100% of full colour
			// B7    = 0 : No blinking
			//       = 1 : Blinking
			//
			arg_len = -1; // Ignore the rest of PDU. FIXME
			break;
			
		case FNC_LISTDATA:
			// 
			// Function: Send list strings to be stored and displayed
            //
			// Return signals: None
			//
			// D(n) 8, ! D8 H FNC (Function code)
			// D(n+1) 8, ! XX H Number of strings to update
			// D(n+2) 8, ! XX H List buffer position for string 1
			// D(n+3) 8, ! XX H Number of alphabets in string 1
			// D(n+4) 8, ! XX H Alphabet 1 in string1
			// D(n+5) 8, ! XX H Num of characters for alphabet 1 in string 1
			// D(n+6) 8, ! XX H Char 1 for alphabet 1 in string 1
			// ...
			// D(m+1) 8, ! XX H Alphabet 2 in string1
			// D(m+2) 8, ! XX H Num of characters for alphabet 2 in string 1
			// D(m+3) 8, ! XX H Char 1 for alphabet 2 in string 1
			// ...
			// D(p+1) 8, ! XX H List buffer position for string 2
			// D(p+2) 8, ! XX H Number of alphabets in string 2
			// D(p+3) 8, ! XX H Alphabet 1 in string2
			// D(p+4) 8, ! XX H Num of characters for alphabet 1 in string 2
			// D(p+5) 8, ! XX H Char 1 for alphabet 1 in string 2
			// ...
			// D(q+1) 8, ! XX H Alphabet 2 in string2
			// D(q+2) 8, ! XX H Num of characters for alphabet 2 in string 2
			// D(q+3) 8, ! XX H Char 1 for alphabet 2 in string 2
			// ...		
			arg_len = -1; // Ignore the rest of PDU. FIXME
			break;

		case FNC_SCROLLLIST:
			// 
			// Function: Send list strings order.
            //
			// Return signals: None
			//
			// D(n) 8, ! D9 H FNC (Function code)
			// D(n+1) 16, ! XX H Total number of list elements in the list(LSB first)
			// D(n+3) 16, ! XX H List element position in the total list - scrollbar(LSB
			// first)
			// D(n+5) 8, ! XX H Displayed position to highlight (1-4)
			// D(n+6) 8, ! XX H List buffer position to display first
			// D(n+7) 8, ! XX H First character description (see below)
			// D(n+8) 8, ! XX H List buffer position to display second
			// D(n+9) 8, ! XX H Second character description (see below)
			// D(n+10) 8, ! XX H List buffer position to display third
			// D(n+11) 8, ! XX H Third character description (see below)
			// D(n+12) 8, ! XX H List buffer position to display fourth
			// D(n+13) 8, ! XX H Fourth character description (see below)
			//
			// Detailed description:
			//
			// D(n+6) Number of row / option to display
			// D(n+8) Number of row / option to display
			// D(n+10) Number of row / option to display
			// D(n+12) Number of row / option to display
			//         nn = Display row
			//         FF = Display blank row
			// D(n+7) CHARACTER DESCRIPTION
			// D(n+9) CHARACTER DESCRIPTION
			// D(n+11) CHARACTER DESCRIPTION
			// D(n+13 CHARACTER DESCRIPTION
			// B0-B4 1 = Font small
			//       2 = Font medium
			//       3 = Font large
			// B5-B6 = 0 : 0% of full colour
			//       = 1 : 30% of full colour
			//       = 2 : 60% of full colour
			//       = 3 : 100% of full colour
			// B7 : Not used
			//
			arg_len = -1; // Ignore the rest of PDU. FIXME
			break;

		case FNC_TOPMENUDATA:
			// 
			// Function: Send list strings to be stored and displayed
            //
			// Return signals: None
			//
			// D(n) 8, ! DA H FNC (Function code)
			// D(n+1) 8, ! XX H Number of strings to update
			// D(n+2) 8, ! XX H List buffer position for string 1
			// D(n+3) 8, ! XX H Number of alphabets in string 1
			// D(n+4) 8, ! XX H Alphabet 1 in string 1
			// D(n+5) 8, ! XX H Num of chars for alphabet 1 in string 1
			// D(n+6) 8, ! XX H Char 1 for alphabet 1 in string 1
			// ...
			// D(p+1) 8, ! XX H List buffer position for string 2
			// D(p+2) 8, ! XX H Number of alphabets in string 2
			// D(p+3) 8, ! XX H Alphabet 1 in string 2
			// D(p+4) 8, ! XX H Num of chars for alphabet 1 in string 2
			// D(p+5) 8, ! XX H Char 1 for alphabet 1 in string 2
			// ...
			//
            arg_len = -1; // Ignore the rest of PDU. FIXME
            break;

		case FNC_SCROLLTOPMENU:
			//
			// Function: Highlight a top menu heading.
            //
			// Return signals: None
			//
			// D(n) 8, ! DB H FNC (Function code)
			// D(n+1) 8, ! XX H Number of column / heading to highlight,
			// one of 0 - 5
			//
			arg_len = -1; // Ignore the rest of PDU. FIXME
			break;

		case FNC_SCROLLSUBMENU:
			// 
			// Function: Highlight an option under a top menu heading.
            //
			// Return signals: None
			//
			// D(n) 8, ! DC H FNC (Function code)
			// D(n+1) 8, ! XX H Total number of options
			// D(n+2) 8, ! XX H Position of option in whole list - scrollbar
			// D(n+3) 8, ! XX H Number of displayed element to highlight (1-4)
			// D(n+4) 8, ! XX H Number of row/option to display first,
			//             FF = display blank row
			// D(n+5) 8, ! XX H Number of row/option to display second,
			//             FF = display blank row
			// D(n+6) 8, ! XX H Number of row/option to display third,
			//             FF = display blank row
			// D(n+7) 8, ! XX H Number of row/option to display fourth,
			//             FF = display blank row
			//
            arg_len = -1; // Ignore the rest of PDU. FIXME
            break;

        case FNC_EQULOOP:
            //
            // Function: The signal controls loop back activation and deactivation of the
            // available loops in the equipment. These are described below.
            //
            // Return signals: EQUSTA
            //
            // D(n)   8, ! E0 H FNC (Function code)
            // D(n+1) 8, ! XX H Loop data
            //
            // Detailed description:
            //
            // D(n+1) LOOP DATA
            //   B2 : 0 = Deactivate loop of B1 channel to line
            //      : 1 = Activate loop of B1 channel back to line
            //   B3 : 0 = Deactivate loop of B2 channel to line
            //      : 1 = Activate loop of B2 channel back to line
            //
            arg_len = 1;
            break;

        case FNC_EQUTESTREQ:
        	//
            // Function: The signal requests test of different functions in the instrument. The
            // type of test is specified by the test code which is associated with the
            // function code.
            //
            // Return signals: EQUTESTRES (depending on code)
            //
            arg_len = -1; // Ignore the rest of PDU. FIXME
            break;

        case FNC_NULLORDER:
            arg_len = 0;
            break;

        default:
        	//
            // Unknown FNC or not properly parsed.
            //
      		arg_len = -1; // Ignore the rest of PDU.
            break;
        }
    
    if ( arg_len < 0 )
    	return;

	// Find next FNC
	//
	++arg_len;
	data += arg_len;
	data_len -= arg_len;

	goto PARSE_NEXT_FNC;
    }
