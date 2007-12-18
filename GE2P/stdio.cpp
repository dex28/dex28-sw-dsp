
#include "DSP.h"
#include "MsgBuf.h"

#pragma DATA_SECTION(".msgbuf")
MSGBUF inBuf;

#pragma DATA_SECTION(".msgbuf")
MSGBUF outBuf;

/////////////////////////////////////////////////////////////////////////////

void Init_STDIO( void )
{
    *(MSGBUF**)0x007A = &outBuf;
    *(MSGBUF**)0x007B = &inBuf;
    
	// EnableIRQ( IRQMASK_HINT );
    }

/////////////////////////////////////////////////////////////////////////////

#include <cstdarg>

void tracef( const char* format... )
{
    static const char hextab [] = "0123456789ABCDEF";
	unsigned char* outStr = (unsigned char*)vpoStack_ptr - 200;

    std::va_list marker;
    va_start( marker, format ); // Initialize variable arguments.

	int len = 0;

	while( *format )
	{
		if ( *format != '%' )
		{
			outStr[ len++ ] = *format++;
			continue;
			}

		format++; // Advance to next character

		if ( *format == '%' )
		{
			outStr[ len++ ] = '%';
			}
		else if ( *format == 'c' )
		{
			unsigned int c = va_arg( marker, unsigned int );
			outStr[ len++ ] = hextab[ ( c >>  4 ) & 0xF ];
			outStr[ len++ ] = hextab[ ( c       ) & 0xF ];
			}
		else if ( *format == 'a' )
		{
			unsigned int* c = va_arg( marker, unsigned int* );
			int c_count = va_arg( marker, int );
			for( ; c_count > 0; c_count--, c++ )
			{
				outStr[ len++ ] = hextab[ ( *c >>  4 ) & 0xF ];
				outStr[ len++ ] = hextab[ ( *c       ) & 0xF ];
				if ( c_count > 1 )
					outStr[ len++ ] = ' ';
				}
			}
		else if ( *format == 'x' )
		{
			unsigned int x = va_arg( marker, unsigned int );
			outStr[ len++ ] = hextab[ ( x >> 12 ) & 0xF ];
			outStr[ len++ ] = hextab[ ( x >>  8 ) & 0xF ];
			outStr[ len++ ] = hextab[ ( x >>  4 ) & 0xF ];
			outStr[ len++ ] = hextab[ ( x       ) & 0xF ];
			}
		else if ( *format == 'l' )
		{
			unsigned long x = va_arg( marker, unsigned long );
			outStr[ len++ ] = hextab[ ( x >> 28 ) & 0xF ];
			outStr[ len++ ] = hextab[ ( x >> 24 ) & 0xF ];
			outStr[ len++ ] = hextab[ ( x >> 20 ) & 0xF ];
			outStr[ len++ ] = hextab[ ( x >> 16 ) & 0xF ];
			outStr[ len++ ] = hextab[ ( x >> 12 ) & 0xF ];
			outStr[ len++ ] = hextab[ ( x >>  8 ) & 0xF ];
			outStr[ len++ ] = hextab[ ( x >>  4 ) & 0xF ];
			outStr[ len++ ] = hextab[ ( x       ) & 0xF ];
			}
		else if ( *format == 's' )
		{
			const char* str = va_arg( marker, const char* );
			while( *str )
				outStr[ len++ ] = *str++;
			}

		format++; // Advance to next character
		}

	outStr[ len++ ] = '\n';
	outStr[ len ] = 0;

   	va_end( marker ); // Reset variable arguments.

   	outBuf.PutPack( MSGTYPE_STDIO, outStr, len );
	}

