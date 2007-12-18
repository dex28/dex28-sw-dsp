#ifndef _B_CHANNEL_H_INCLUDED
#define _B_CHANNEL_H_INCLUDED

extern "C"
{
// D2-Tech ECMR
#include <comm.h>
#include <ecmr.h>
#include <fmtd.h>
#include <nfe.h>
// D2-Tech G726 CODEC
#include <g726.h>
// D2-Tech G729ab CODEC
#include <g729ab.h>
}

#include "g711.h"

enum { MAX_BLOCK_SIZE = 80 }; // in samples

enum { MAX_FRAME_SIZE = 80 }; // in samples

class B_IO_Buffer
{
	bool loopback;
	int maxp;
	int curp;
	int bufp;
	int RxBuf[ 2 ][ MAX_BLOCK_SIZE ];
	int TxBuf[ 2 ][ MAX_BLOCK_SIZE ];
	
	volatile bool Event_DataReady;

public:

	int GetCurP( void ) const
	{
		DisableIRQs ();
		int x = curp;
		if ( Event_DataReady )
			x += maxp;
		EnableIRQs ();
		return x;
		}

	B_IO_Buffer ()
	{
		loopback = false;
		maxp = MAX_BLOCK_SIZE;
		curp = 0;
		bufp = 0;
		Event_DataReady = false;
		}

	void SetBlockSize( int block_size )
	{
		curp = 0;
		maxp = block_size;
		}

	int GetBlockSize( void ) const
	{
		return maxp;
		}

	bool IsLoopback( void ) const
	{
		return loopback;
		}

	void SetLoopback( bool v )
	{
		loopback = v;
		}

	int McBSP_IO( int data )
	{
		if ( loopback )
		{
			RxBuf[ bufp ][ curp ] = TxBuf[ bufp ][ curp ];
			}
		else
		{
			RxBuf[ bufp ][ curp ] = -data >> 3;
			data = -TxBuf[ bufp ][ curp ] << 3;
			}

		if ( ++curp >= maxp )
		{
			curp = 0;
			bufp = 1 - bufp;
			Event_DataReady = true;
			}
			
		return data;
		}

	bool IsTriggered_DataReady( void )
	{
		if ( ! Event_DataReady )
			return false;
			
		Event_DataReady = false;
		return true;
		}

	void ClearTxBoth( int value = 0 )
	{
		memset( TxBuf[ 0 ], value, maxp );
		memset( TxBuf[ 1 ], value, maxp );
		}

	void ClearTxBuffer( int value = 0 )
	{
		memset( TxBuf[ 1 - bufp ], value, maxp );
		}

	void PlayAgain( void )
	{
		memcpy( TxBuf[ 1 - bufp ], TxBuf[ bufp ], maxp );
		}

	int* Tx_Buffer( void )
		{ return &TxBuf[ 1 - bufp ][ 0 ]; }
		
	int* Rx_Buffer( void)
		{ return &RxBuf[ 1 - bufp ][ 0 ]; }
	};

enum { JITBUF_MAXSIZE = 16 }; // must be 2^n
	
class B_JitterBuffer
{
	friend int main( void );
	friend class B_Channel;

	unsigned short seqNo[ JITBUF_MAXSIZE ];
	
	unsigned short codew[ JITBUF_MAXSIZE ][ 80 ]; // ENCODED/COMPRESSED data is here !
	int payload[ JITBUF_MAXSIZE ];

public:

	unsigned int packets_count;
	
	long RL_avg; // average difference Remote - Local TS
	long RL_var; // variance of difference Remote - Local TS
	long JLen;   // Jitter Buffer Length
	
	int JAlpha;  // Exponential Average Constant
	int JBeta;   // Jitter Variation Gain Cofficient
    int JLenMin; // Minimum Jitter Buffer Length
    int JLenMax; // Maximum Jitter Buffer Length

	B_JitterBuffer( void )
	{
		RL_avg = 0;
		RL_var = 0;
		JLen = 0;

		JLenMin = 0;
		JLenMax = 200;
		JAlpha = 4; // PARAMETER: Exponential Average Constant
		JBeta = 4; // PARAMETER: Jitter Variation Gain Coefficient

		packets_count = 0;

		for ( int i = 0; i < JITBUF_MAXSIZE; i++ )
		{
			seqNo[ i ] = 0;
			payload[ i ] = -1;
			}
		}

	void Put( unsigned long L_ts, unsigned char* pkt, int pkt_len );

	void ClearAll( void )
	{
		packets_count = 0;

		for ( int i = 0; i < JITBUF_MAXSIZE; i++ )
		{
			seqNo[ i ] = 0;
			payload[ i ] = -1;
			}
		}
	};

class B_Channel
{
	friend int main( void );

	int id;
	class XHFC_Port* D_Ch;
	bool inSpeech;

	unsigned short localSeqNo;
    unsigned short playRemoteSeqNo;

	int curJitBuf;

	unsigned short out_pkt[ 2 + 80 ]; // max output packet
	int encoding_PT;
	int decoding_PT;

	int cur_block;
	int cur_frame;
	int block_size; // must be multiple of 4 
	int frame_size;
	int pkt_size;

	int txFrameBuf [ MAX_FRAME_SIZE ]; // input buffer; samples
	int rxFrameBuf [ MAX_FRAME_SIZE ]; // output buffer; samples

	// LEC (ECMR)
	//	
	int nlpOutBuf [ MAX_BLOCK_SIZE ]; 
	int delayBuf  [ ECMR_128MS_DLY_BUF_SIZE ];

	int ec_control; // default control word
	NFE_Obj  nfe_obj;
	FMTD_Obj fmtd_obj1;
	FMTD_Obj fmtd_obj2;
	ECMR_Obj ec_obj;

	union
	{
		G726_EObj   enc_obj;
		G729AB_EObj enc_obj7;
		};
		
	union
	{
		G726_DObj   dec_obj;
		G729AB_DObj dec_obj7;
		};

	unsigned short b_trace[ 321 ];

	struct RequestedParameters
	{
		// Flags:
		bool compressed_RTP  ;
		bool trace_pcm       ;
		bool alaw_companding ;
		bool LEC_enabled     ;
		bool NLP_enabled     ;
		bool FMTD_enabled    ;
		bool LEC_WarmStart   ;
		
		int encoding_PT;
		int packet_size;
		
		int LEC_reinit; // 0 = none, 1 = cold, 2 = warm
		// LEC: requires LEC_init():
		int LEC_TailLen;
		int LEC_MinERL; // 0, 3 or 6 (dB)
		int NFE_MinNoiseFloor;
		int NFE_MaxNoiseFloor;
		int NLP_MinVoicePower;
		int NLP_HangoverTime;
		int block_size;
		int ISR_block_size;
		unsigned int p0DB; // Reference input level for 0dBm
		// p0DB = A * 10^( -P/20) / sqrt(2)
		};

public:

	RequestedParameters param;

	unsigned int tx_depleted_count; // tx_buf missed blocks
	unsigned int pred_adjusts_count;
	unsigned int packets_count;
	
	B_IO_Buffer io_buf;     // accessed by IRQ handler
	B_JitterBuffer jit_buf; // accessed by main ()

	B_Channel( void )
		: id( -1 )
		, D_Ch( NULL )
	{
		inSpeech = false;
		localSeqNo = 0;

		playRemoteSeqNo = 0;
		curJitBuf = -1;

		cur_block = 0;
		cur_frame = 0;
		block_size = 40; // PARAMETER: Block Size (in samples)
		frame_size = 1; // PARAMETER: Frame Size (in blocks)
		pkt_size = 4; // PARAMETER: Packet Size (in frames)

		encoding_PT = 0; // by default G.711 A-Law
		decoding_PT = 0; // by default G.711 A-Law

		io_buf.SetBlockSize( block_size );

		tx_depleted_count = 0;
		pred_adjusts_count = 0;
		packets_count = 0;

		ec_control = ECMR_ERL2 | ECMR_NLP;

		param.trace_pcm = false;
		param.alaw_companding = false;

		param.encoding_PT = encoding_PT;
		param.packet_size = 160;
		param.compressed_RTP = true;

		param.LEC_reinit = 0;

		param.p0DB = 2018; // 0dBm0, assuming 13-bit linear pcm
		param.block_size = 40;
		param.ISR_block_size = 0;
		param.LEC_enabled = true;
		param.LEC_WarmStart = true;
		param.NLP_enabled = true;
		param.FMTD_enabled = true;
		param.LEC_TailLen = 1024; // 128 ms
		param.LEC_MinERL = 6; // dB
		param.NFE_MinNoiseFloor = NFE_MINNOISE_DEF;
		param.NFE_MaxNoiseFloor = NFE_MAXNOISE_DEF;
		param.NLP_MinVoicePower = ECMR_MIN_VOICE_PWR_DEF;
		param.NLP_HangoverTime = ECMR_VOICE_HNG_CNT_DEF;
		}

	void Initialize( int p_id, XHFC_Port* owner, bool ALaw = true );
	
	void DataReady_EH( void );

	void StartTransmission( void );
	void StopTransmission( void );

	void LEC_Init( bool warmstart );
	void InitEncoder( int payload );

	void ChangeParameters( unsigned char* data, int data_len );
	
	unsigned long GetLocalTS( void )
	{
		return (unsigned long)localSeqNo * 160 + cur_block * 40 + io_buf.GetCurP ();
		}
	};

#endif // _B_CHANNEL_H_INCLUDED
