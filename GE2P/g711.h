#ifndef _G711_H_INCLUDED
#define _G711_H_INCLUDED

extern void G711_gen4kHz_aLaw( unsigned short* codeBuf, int len );
extern void G711_gen1kHz_aLaw( unsigned short* codeBuf, int len );

extern void G711_encode_aLaw( unsigned short* codeBuf, const int* srcBuf, int len );
extern void G711_decode_aLaw( int* destBuf, const unsigned short* codeBuf, int len );

extern void G711_encode_uLaw( unsigned short* codeBuf, const int* srcBuf, int len );
extern void G711_decode_uLaw( int* destBuf, const unsigned short* codeBuf, int len );

extern int s13_to_aLaw ( int pcm_val );
extern int aLaw_to_s13( int a_val );
extern int s14_to_uLaw( int pcm_val );
extern int uLaw_to_s14( int u_val );

#endif // _G711_H_INCLUDED
