
#include "DSP.h"
#include "MsgBuf.h"
#include "ELU28.h"

///////////////////////////////////////////////////////////////////////////////

bool D_TransmitQueue::PutPDU( unsigned char* data, int len )
{
	if ( disabled )
	{
		SendHostFlowStatus ();
		return false;
		}

    // Check free space. If there is no one, drop PDU.
    //
    int used_space = writep >= readp 
        ? writep - readp
        : writep + sizeof( bufp ) / sizeof( bufp[0] ) - readp;

	if ( flowXON && used_space > 160 )
	{
		flowXON = false;
		SendHostFlowStatus ();
		}

	// Max needed space is approx len/2 + 4 words
	//
	if ( used_space + ( len >> 1 ) >= sizeof( bufp ) / sizeof( bufp[0] ) - 4 )
	{
		++dropped_counter;
		SendHostFlowStatus ();
		return false;
		}

    // Put PDU into output buffer, constantly calculating checksum.
    //
    int cksum = 0;
    
    unsigned short NBYTES = len + 2; // additional size is for NBYTES + CKSUM
    
    // Remove old P & SN bits from OPC
    //
    unsigned short OPC = ( data[ 0 ] & ~0x1E );
    
    // Set SN = curSeqNo
    //
    OPC |= ( curSeqNo << 1 );
    
    // Set P = enhanced_protocol
    //
	// NOTE: It seems that Ericsson DTS firmware *DOES NOT* set P = 1
	// in enhanced mode; However, it send NBYTES as 2 octets.
	//
    if ( enhanced_protocol )
        OPC |= 0x10;

	// Increment packet sequence number counter
	//        
    if ( ++curSeqNo >= 8 )
    	curSeqNo = 0;
    	
    data[ 0 ] = OPC; // Store back OPC to data[]

	int i = 0; // i points to data[i]

	// Begin frame with signal length, followed by NBYTES,
	// and if NBYTES size is 1 octet, by OPC also.
	//
	if ( enhanced_protocol ) // NBYTES requires 2 octets
	{
		++NBYTES; // Increase signal length for the second octet of NBYTES
		*writep++ = NBYTES; // Store signal length
		if ( writep > maxp ) writep = bufp;

		NBYTES |= 0x8000; // Turn on "TWO-OCTET-NBYTES" flag
		unsigned short MSB = ( NBYTES >> 8 );
		unsigned short LSB = ( NBYTES & 0xFF );
		cksum += MSB;
		cksum += LSB;

		*writep++ = ( LSB << 8 ) | MSB;
		if ( writep > maxp ) writep = bufp;
		}
	else // NBYTES is one octet long
	{
		*writep++ = NBYTES; // Store signal length
		if ( writep > maxp ) writep = bufp;
	
		cksum += NBYTES;
		cksum += OPC;
		
		*writep++ = ( OPC << 8 ) | NBYTES;
		if ( writep > maxp ) writep = bufp;
		
		++i; // Skip OPC that we've already used
		}

	// Pack octets into 16-bit words. Bits 15-8 contains LS-byte
	// bits 7-0 should contain MS-byte. Ericsson ELP2B+D protocol
	// specification tells that octet is shifted LS-bit first into
	// D-channel.
	//
    for ( ; i + 1 < len; i += 2 )
    {
        cksum += data[ i ];
        cksum += data[ i + 1 ];

		*writep++ = ( data[ i + 1 ] << 8 ) | data[ i ];
		if ( writep > maxp ) writep = bufp;
        }

    if ( i < len ) // Put last octet followed by checksum
    {
    	cksum += data[ i ];

    	*writep++ = ( ( ( cksum - 1 ) & 0xFF ) << 8 ) | data[ i ];
		if ( writep > maxp ) writep = bufp;
    	}
    else // Put cheksum
    {
		*writep++ = ( ( cksum - 1 ) & 0xFF );
		if ( writep > maxp ) writep = bufp;
    	}

    return true;
    }

///////////////////////////////////////////////////////////////////////////////

ELU28_D_Channel:: ELU28_D_Channel( int p_id, B_Channel* p_b )
    : id( p_id )
    , dasl( p_id )
    , xmt_que( p_id )
    , rcv_buf ()
    , bChannel( p_b )
{
    poll_counter = 0;
    transmission_order = 0;
    IsOWS = false;
    IsCTT10 = false;

	fault_counter = 0;
	timeout_counter = 0;
	timeout_counter_nbytes = 0;
	timeout_counter_pdu = 0;
	timeout_counter_ack = 0;

    packet_header[ 0 ] = id;
    packet_status = PACKET_EMPTY;
    packet_len = 0;
    
    delayed_power_up = false;
    EQUSTA_len = 0;

    oldRcvdSeqNo = 7;
    oldChecksum = 0;

    rcvd_octets = 0;
    cksum = 0;
    NBYTES = 0;
    
	Go_State( DISABLED, -1 );
	SetVerb_State( VERBOSE_DOWN );
    }

///////////////////////////////////////////////////////////////////////////////

void ELU28_D_Channel::Initialize( void )
{
	if ( id == 0 )
		EnableIRQ( IRQMASK_INT0  );
	else if ( id == 1 )
		EnableIRQ( IRQMASK_INT1 );

	bChannel->Initialize( this );

	if ( dasl.IsMaster () )
		dasl.PowerUp ();
		
	DASL_EH ();
    }

///////////////////////////////////////////////////////////////////////////////
