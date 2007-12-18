#ifndef _MSGBUF_H_INCLUDED
#define _MSGBUF_H_INCLUDED

#ifndef NULL
#define NULL (0)
#endif

#define MSGBUF_SIZE (0x800-3) // ~2kBy

enum MSGTYPE
{
	MSGTYPE_NONE     = -1,
	//
	MSGTYPE_STDIO    = 0,
	MSGTYPE_IO_D     = 1,
	MSGTYPE_IO_B     = 2,
	MSGTYPE_WATCHDOG = 3,
	//
	// undefined message types should be skipped and not handled
	//
	MSGTYPE_CTRL  = 7
	};

struct MSGBUF_HEADER
{
    unsigned short bufsize;
    volatile unsigned short readp;
    volatile unsigned short writep;
    };

class MSGBUF
{
    MSGBUF_HEADER header;

    unsigned short buffer[ MSGBUF_SIZE ];

public:

    MSGBUF( void );

    bool Put( MSGTYPE type, const unsigned short* data, int len ); // len in words
    bool PutPack( MSGTYPE type, const unsigned char* data, int len ); // len in octets
    bool Put( const char* data, int len = -1 /*asciiz*/ ); // STDIO, packed

	MSGTYPE GetUnpack( int& len, unsigned char* data, int max_len );
	
	bool IsEmpty( void )
	{
		return header.readp == header.writep;
		}
    };

extern void Init_STDIO( void );

extern MSGBUF outBuf, inBuf;

extern void tracef( const char* format... );

#endif // _MSGBUF_H_INCLUDED
