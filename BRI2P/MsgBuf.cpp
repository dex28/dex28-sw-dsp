
#include "DSP.h"
#include "MsgBuf.h"

MSGBUF::MSGBUF( void )
{
    header.bufsize = sizeof( buffer ) / sizeof( unsigned short );
    header.readp = 0;
    header.writep = 0;
    }

bool MSGBUF::Put( const char* data, int len )
{
    if ( len < 0 ) // Zero terminated ASCII
    {
    	// Calculate length until '\0'
    	for ( len = 0; data[ len ]; len++ )
			{}
		}

	if ( len <= 0 )
		return true;
		
	return PutPack( MSGTYPE_STDIO, (unsigned const char*)data, len );
    }

bool MSGBUF::PutPack( MSGTYPE msg_type, const unsigned char* data, int len )
{
	if ( len > 0x400 )
		return false; // Maximum message size (1024 octets) exceeded

    // Check if there is free space for new data
    //
    unsigned short readp = header.readp;
    unsigned short writep = header.writep;

    int freespace = writep >= readp ? writep - readp : writep + header.bufsize - readp;

    if ( freespace + len/2 + 5 >= header.bufsize - 1 )
        return false; // there is no free space

    // Set MSBits 14..12 to contain message type.
    // Store length in LSBits 10..0 indicting number of 8-bit bytes.
    //
    buffer[ writep ] = len | ( (unsigned short)msg_type << 12 );
    if ( ++writep >= header.bufsize )
        writep = 0;

    // Pack uint8 data into uint16 words. MSB first!
    //
    for ( int i = 0; i < len; i += 2 )
    {
    	unsigned short packed = ( ( data[ i ] & 0xFF ) << 8 );
    	if ( i + 1 < len )
    		packed |= ( data[ i + 1 ] & 0xFF );
    	
        buffer[ writep ] = packed;

        if ( ++writep >= header.bufsize )
            writep = 0;
        }

    // Update circular buffer 'head' pointer
    //
    header.writep = writep;

	HPIC = 0x0A; // Assert HINT

    return true;
    }

bool MSGBUF::Put( MSGTYPE msg_type, const unsigned short* data, int len )
{
	if ( len > 0x200 )
		return false; // Maximum message size (1024 octets) exceeded
		
    // Check if there is free space for new data
    //
    unsigned short readp = header.readp;
    unsigned short writep = header.writep;

    int freespace = writep >= readp ? writep - readp : writep + header.bufsize - readp;

    if ( freespace + len + 5 >= header.bufsize - 1 )
        return false; // there is no free space

    // Set MSBits 14..12 to contain message type.
    // Store length in LSBits 10..0 indicating number of octets.
    //
    buffer[ writep ] = ( len * 2 ) | ( (unsigned short)msg_type << 12 );
    if ( ++writep >= header.bufsize )
        writep = 0;

    // Put data into buffer
    //
    for ( int i = 0; i < len; i ++ )
    {
        buffer[ writep ] = data[ i ];

        if ( ++writep >= header.bufsize )
            writep = 0;
        }

    // Update circular buffer 'head' pointer
    //
    header.writep = writep;
    
	HPIC = 0x0A; // Assert HINT
    
    return true;
    }

MSGTYPE MSGBUF::GetUnpack( int& len, unsigned char* data, int max_len )
{
    // Check wether buffer contains any data
    //
    if ( header.readp == header.writep )
        return MSGTYPE_NONE; // No data
        
    // Get available data
    //
    unsigned short readp = header.readp;

    unsigned short typeAndLen = buffer[ readp ];
    if ( ++readp >= header.bufsize )
        readp = 0;

	// bits 10..0 contain length
	// bits 15..12 contain msg type
	//
	len = typeAndLen & 0x07FF;

	if ( len < max_len )
		max_len = len;

	// Get unpack data uint16 -> uint8 MSB, LSB
	//
    for ( int i = 0; i < max_len; i += 2 )
    {
        *data++ = ( buffer[ readp ] >> 8 ) & 0xFF;
        if ( i + 1 < len )
            *data++ = buffer[ readp ] & 0xFF;
        
        if ( ++readp >= header.bufsize )
            readp = 0;
        }
        
    // Skip data that cannot fit users data[]
    //
    for ( int i = len; i < max_len; i += 2 )
    {
        if ( ++readp >= header.bufsize )
            readp = 0;
        }

    // Update circular buffer 'tail' pointer
    //
    header.readp = readp;
    
	return MSGTYPE( ( typeAndLen >> 12 ) & 0x07 ); // bits 14..12
	}
