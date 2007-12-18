
#include "g711.h"
  
int s13_to_aLaw( int linval )
{
	// 0 <= ix < 2048
	int ix = linval < 0 
		? ~linval >> 1       // 1's complement for negative values
		: linval >> 1;

    // Do more, if exponent > 0.
    // exponent=0 for ix <= 15.
    if (ix > 15)
    {
		// first step: find mantissa and exponent
      	int iexp = 1;
      	while ( ix > 16 + 15 )
      	{
        	ix >>= 1;
        	++iexp;
      		}
      	// second step: remove leading '1'
      	ix -= 16;
      	// now compute encoded value
      	ix |= ( iexp << 4 );
    	}
    	
    // add sign bit
    if ( linval >= 0 )
      ix |= 0x80;

    return ix ^ 0x55;  // toggle even bits
	}

int aLaw_to_s13( int logval )
{
    int ix = logval ^ (0x0055);  // re-toggle toggled bits
    ix &= (0x007F);             // remove sign bit
    int iexp = ix >> 4;             // extract exponent
    int mant = ix & (0x000F);       // now get mantissa
    if (iexp > 0)
      mant = mant + 16;         // add leading '1', if exponent > 0

    mant = (mant << 1) + 1;      // now mantissa left justified and
    // 1/2 quantization step added
    if (iexp > 1)               // now left shift according exponent
      mant = mant << (iexp - 1);

    return logval > 127 // invert, if negative sample
      ? mant
      : -mant;
}

int s14_to_uLaw( int linval )
{
    // --------------------------------------------------------------------
    // Change from 14 bit left justified to 14 bit right justified
    // Compute absolute value; adjust for easy processing
    // --------------------------------------------------------------------
    int absno = linval < 0       // compute 1's complement in case of 
      ? ((~linval)) + 33// negative samples
      : ((linval)) + 33;// NB: 33 is the difference value
    // between the thresholds for
    // A-law and u-law.
    if (absno > (0x1FFF))       // limitation to "absno" < 8192
      absno = (0x1FFF);

    // Determination of sample's segment
    int i = absno >> 6;
    int segno = 1;
    while (i != 0)
    {
      segno++;
      i >>= 1;
    }

    // Mounting the high-nibble of the log-PCM sample
    int high_nibble = (0x0008) - segno;

    // Mounting the low-nibble of the log PCM sample
    int low_nibble = (absno >> segno)       // right shift of mantissa and
      & (0x000F);               // masking away leading '1'
    low_nibble = (0x000F) - low_nibble;

    // Joining the high-nibble and the low-nibble of the log PCM sample
    int logval = (high_nibble << 4) | low_nibble;

    // Add sign bit
    if (linval >= 0)
      logval = logval | (0x0080);
    return logval;
}

int uLaw_to_s14( int logval )
{
    int sign = logval < (0x0080) // sign-bit = 1 for positiv values
      ? -1
      : 1;
    int mantissa = ~logval;      // 1's complement of input value
    int exponent = (mantissa >> 4) & (0x0007);      // extract exponent
    int segment = exponent + 1;     // compute segment number
    mantissa = mantissa & (0x000F);     // extract mantissa

    // Compute Quantized Sample (14 bit left justified!)
    int step = (4) << segment;      // position of the LSB
    // = 1 quantization step)
    int linval = sign *          // sign
      (((0x0080) << exponent)   // '1', preceding the mantissa
       + step * mantissa        // left shift of mantissa
       + step / 2               // 1/2 quantization step
       - 4 * 33
      );
    return linval >> 2;
}

///////////////////////////////////////////////////////////////////////////////

void G711_gen4kHz_aLaw( unsigned short* codeBuf, int len )
{
    for ( int i = 0; i < len; i += 2 )
    {
    	*codeBuf++ = 0xaa2a;
    	}
	}

///////////////////////////////////////////////////////////////////////////////

void G711_gen1kHz_aLaw( unsigned short* codeBuf, int len )
{
    for ( int i = 0; i < len; i += 8 )
    {
    	*codeBuf++ = 0x3421;
    	*codeBuf++ = 0x2134;
    	*codeBuf++ = 0xb4a1;
    	*codeBuf++ = 0xa1b4;
    	}
	}

///////////////////////////////////////////////////////////////////////////////

void G711_encode_aLaw( unsigned short* codeBuf, const int* srcBuf, int len )
{
    for ( int i = 0; i < len; i += 2 )
    {
    	*codeBuf++ = ( s13_to_aLaw( srcBuf[ i ] ) << 8 ) 
    		       | s13_to_aLaw( srcBuf[ i + 1 ] );
    	}
	}

///////////////////////////////////////////////////////////////////////////////

void G711_decode_aLaw( int* destBuf, const unsigned short* codeBuf, int len )
{
	for ( int i = 0; i < len; i += 2 )
	{
		*destBuf++ = aLaw_to_s13( ( *codeBuf >> 8 ) & 0xFF ); // MSB
		*destBuf++ = aLaw_to_s13( ( *codeBuf++ ) & 0xFF ); // LSB
		}
	}

///////////////////////////////////////////////////////////////////////////////

void G711_encode_uLaw( unsigned short* codeBuf, const int* srcBuf, int len )
{
    for ( int i = 0; i < len; i += 2 )
    {
    	*codeBuf++ = ( s14_to_uLaw( srcBuf[ i ] << 1 ) << 8 ) 
    		       | s14_to_uLaw( srcBuf[ i + 1 ] << 1 );
    	}
	}

///////////////////////////////////////////////////////////////////////////////

void G711_decode_uLaw( int* destBuf, const unsigned short* codeBuf, int len )
{
	for ( int i = 0; i < len; i += 2 )
	{
		*destBuf++ = uLaw_to_s14( ( *codeBuf >> 8 ) & 0xFF ) >> 1; // MSB
		*destBuf++ = uLaw_to_s14( ( *codeBuf++ ) & 0xFF ) >> 1; // LSB
		}
	}
