
#include "DSP.h"
#include "MsgBuf.h"
#include "xhfc.h"

#pragma DATA_SECTION(".b1mem")
static int g729_B1mem[ sizeof(_G729AB_B1mem) ];

///////////////////////////////////////////////////////////////////////////////

void B_Channel::Initialize( int p_id, XHFC_Port* owner, bool ALaw )
{
	id = p_id;
	D_Ch = owner;
	param.alaw_companding = ALaw;
	
	LEC_Init( /*warmstart*/ false );
    }

void B_Channel::DataReady_EH( void )
{
	if ( ! io_buf.IsTriggered_DataReady () )
		return;

	// If not in speech, ignore received samples, 
	// and set clear next block of samples that shouldb e transmitted.
	//
	if ( ! inSpeech )
	{
		io_buf.ClearTxBuffer ();
		return;
		}

	int* rx_buf = io_buf.Rx_Buffer (); // last received block of samples
	int* tx_buf = io_buf.Tx_Buffer (); // block of samples that should be filled in

	///////////////////////////////////////////////////////////////////////////
	// CODEC -- Decode PCM from Jitter Buffer, i.e. VoIP aka Far End
	//
    if ( curJitBuf >= 0 && jit_buf.seqNo[ curJitBuf ] == playRemoteSeqNo )
    {
    	int payload = jit_buf.payload[ curJitBuf ];

		///////////////////////////////////////////////////////////////////////
    	if ( payload == 0 || payload >= 250 ) // G.711 A-Law
    	{
    		if ( payload >= 250 ) payload = 0;
	    	int CFW = block_size * 8 / 16; // word count in coded frame
	    	unsigned short* ptr = jit_buf.codew[ curJitBuf ] + cur_block * CFW;

			G711_decode_aLaw( tx_buf, ptr, block_size );
			}
		///////////////////////////////////////////////////////////////////////
    	else if ( payload == 1 ) // G.711 u-Law
    	{
	    	int CFW = block_size * 8 / 16; // word count in coded frame
	    	unsigned short* ptr = jit_buf.codew[ curJitBuf ] + cur_block * CFW;

			G711_decode_uLaw( tx_buf, ptr, block_size );
			}
		///////////////////////////////////////////////////////////////////////
    	else if ( payload == 2 ) // G.726-40
    	{
    		if ( payload != decoding_PT )
    		{
    			decoding_PT = payload; // change current decoder
			    G726_init( &dec_obj, G726_DECODE, vpoStack_ptr, block_size, 5, 0 );
    			}

	    	int CFW = ( block_size * 5 + 15 ) / 16; // word count in coded frame
	    	unsigned short* ptr = jit_buf.codew[ curJitBuf ] + cur_block * CFW;

			dec_obj.src_ptr = (int*)ptr;
			dec_obj.dst_ptr = tx_buf;
			G726_decode( &dec_obj );
			}
		///////////////////////////////////////////////////////////////////////
    	else if ( payload == 3 ) // G.726-32
    	{
    		if ( payload != decoding_PT )
    		{
    			decoding_PT = payload; // change current decoder
			    G726_init( &dec_obj, G726_DECODE, vpoStack_ptr, block_size, 4, 0 );
    			}

	    	int CFW = block_size * 4 / 16; // word count in coded frame
	    	unsigned short* ptr = jit_buf.codew[ curJitBuf ] + cur_block * CFW;

			dec_obj.src_ptr = (int*)ptr;
			dec_obj.dst_ptr = tx_buf;
			G726_decode( &dec_obj );
			}
		///////////////////////////////////////////////////////////////////////
    	else if ( payload == 4 ) // G.726-24
    	{
    		if ( payload != decoding_PT )
    		{
    			decoding_PT = payload; // change current decoder
			    G726_init( &dec_obj, G726_DECODE, vpoStack_ptr, block_size, 3, 0 );
    			}

	    	int CFW = ( block_size * 3 + 15 ) / 16; // word count in coded frame
	    	unsigned short* ptr = jit_buf.codew[ curJitBuf ] + cur_block * CFW;

			dec_obj.src_ptr = (int*)ptr;
			dec_obj.dst_ptr = tx_buf;
			G726_decode( &dec_obj );
			}
		///////////////////////////////////////////////////////////////////////
    	else if ( payload == 5 ) // G.726-16
    	{
    		if ( payload != decoding_PT )
    		{
    			decoding_PT = payload; // change current decoder
			    G726_init( &dec_obj, G726_DECODE, vpoStack_ptr, block_size, 2, 0 );
    			}

	    	int CFW = block_size * 2 / 16; // word count in coded frame
	    	unsigned short* ptr = jit_buf.codew[ curJitBuf ] + cur_block * CFW;

			dec_obj.src_ptr = (int*)ptr;
			dec_obj.dst_ptr = tx_buf;
			G726_decode( &dec_obj );
			}
		///////////////////////////////////////////////////////////////////////
    	else if ( payload == 6 || payload == 7 ) // G.729A
    	{
    		if ( payload != decoding_PT )
    		{
    			decoding_PT = payload; // change current decoder
		    	G729AB_init( &dec_obj7, G729AB_DECODE, vpoStack_ptr, g729_B1mem );
    			}

    		if ( cur_block == 0 || cur_block == 2 )
    		{
		    	unsigned short* ptr = jit_buf.codew[ curJitBuf ];
		    	int CWS = 0;
		    	if ( cur_block == 0 )
		    	{
		    		CWS = ptr[ 0 ] >> 8;
		    		++ptr; // skip CWSs
		    		}
		    	else
		    	{
		    		CWS = ptr[ 0 ] & 0xFF;
		    		ptr += 1 + ( ptr[ 0 ] >> 8 ); // skip CWSs and first frame
		    		}

				dec_obj7.src_ptr = (int*)ptr;
				dec_obj7.dst_ptr = txFrameBuf;
				dec_obj7.bfi = 0;
				dec_obj7.bad_lsf = 0;
				dec_obj7.nWords = CWS;
				G729AB_decode( &dec_obj7 );

				// First block in frame
				memcpy( tx_buf, txFrameBuf, block_size * sizeof( int ) );
				}
			else
			{
				// Second block in frame
				memcpy( tx_buf, txFrameBuf + block_size, block_size * sizeof( int ) );
				}

			for ( int i = 0; i < block_size; i++ )
				tx_buf[ i ] >>= 1;
			}
		///////////////////////////////////////////////////////////////////////
		else // Payload mismatch
		{
			io_buf.ClearTxBuffer (); // Play silence
			}
		}
	else // not arrived packet
	{
		++tx_depleted_count;

		if ( decoding_PT >= -4 )
		{
			io_buf.PlayAgain (); // Play last block again

			if ( decoding_PT >= 0 )
				decoding_PT = -1;
			else
				--decoding_PT; // number of consequtive lost blocks
			}
		else
		{
			io_buf.ClearTxBuffer (); // Play silence
			}
		}
		
	///////////////////////////////////////////////////////////////////////////
	// Line Echo Cancellation
	// Only on Gateway (Switch Side), i.e. Switch is Near End for Hybrid Echo
	//
	if ( D_Ch && param.LEC_enabled )
	{
		ec_obj.rin_ptr = tx_buf;
		ec_obj.sin_ptr = rx_buf;
	    ec_obj.control = ec_control;

	    ///////////////////////////////////////////////////////////////////////
	    // Determine if canceller should be bypassed.
	    //
    	if ( param.FMTD_enabled
    		&& ( ( fmtd_obj1.detected & FMTD_PHASE_WORD ) 
    		     || ( fmtd_obj2.detected & FMTD_PHASE_WORD ) 
    		   )
    		) 
	    {
	        ec_obj.control |= ECMR_BYP; // Bypass if Fax/Modem tone detected
	    }

	    ///////////////////////////////////////////////////////////////////////
	    // Initialize ECMR soutFull_ptr.
	    // Note: soutFull_ptr buffer can be shared by different channels
	    //
		static int soutBuf [ MAX_BLOCK_SIZE ];
	    ec_obj.soutFull_ptr = soutBuf;

	    ///////////////////////////////////////////////////////////////////////
	    // Initialize NFE and FMTD source pointers
	    //
	    nfe_obj.src_ptr   = ec_obj.soutFull_ptr;
	    fmtd_obj1.src_ptr = ec_obj.soutFull_ptr;
	    fmtd_obj2.src_ptr = ec_obj.rin_ptr;

	    ///////////////////////////////////////////////////////////////////////
	    // Run modules in this order
	    //
	    ECMR_run( &ec_obj, &nfe_obj );
	    NFE_run( &nfe_obj );
	    FMTD_fullDuplexDetect( &fmtd_obj1, &fmtd_obj2 );

		rx_buf = nlpOutBuf; // rx_buf from now on points to output from ECMR NLP
		}

	///////////////////////////////////////////////////////////////////////////
	// CODEC -- Encode PCM and store block in outbound RTP packet
	//
	int payload_size = 0;

	///////////////////////////////////////////////////////////////////////////
	if ( encoding_PT == 0 || encoding_PT >= 250 ) // G.711 A-Law
	{
		int CFW = block_size * 8 / 16; // word count in coded frame
		unsigned short* ptr = out_pkt + 2 + cur_block * CFW;
		payload_size = pkt_size * CFW;

		G711_encode_aLaw( ptr, rx_buf, block_size );
/*
		if ( id == 0 && ! D_Ch->dasl.IsMaster () )
			G711_gen4kHz_aLaw( ptr, block_size );
*/			
		}
	///////////////////////////////////////////////////////////////////////////
	else if ( encoding_PT == 1 ) // G.711 u-Law
	{
		int CFW = block_size * 8 / 16; // word count in coded frame
		unsigned short* ptr = out_pkt + 2 + cur_block * CFW;
		payload_size = pkt_size * CFW;

		G711_encode_uLaw( ptr, rx_buf, block_size );
		}
	///////////////////////////////////////////////////////////////////////////
	else if ( encoding_PT == 2 ) // G.726-40
	{
		int CFW = ( block_size * 5 + 15 ) / 16; // word count in coded frame
		unsigned short* ptr = out_pkt + 2 + cur_block * CFW;
		payload_size = pkt_size * CFW;

		enc_obj.src_ptr = rx_buf;
		enc_obj.dst_ptr = (int*)ptr;
		G726_encode( &enc_obj );
		}
	///////////////////////////////////////////////////////////////////////////
	else if ( encoding_PT == 3 ) // G.726-32
	{
		int CFW = block_size * 4 / 16; // word count in coded frame
		unsigned short* ptr = out_pkt + 2 + cur_block * CFW;
		payload_size = pkt_size * CFW;

		enc_obj.src_ptr = rx_buf;
		enc_obj.dst_ptr = (int*)ptr;
		G726_encode( &enc_obj );
		}
	///////////////////////////////////////////////////////////////////////////
	else if ( encoding_PT == 4 ) // G.726-24
	{
		int CFW = ( block_size * 3 + 15 ) / 16; // word count in coded frame
		unsigned short* ptr = out_pkt + 2 + cur_block * CFW;
		payload_size = pkt_size * CFW;

		enc_obj.src_ptr = rx_buf;
		enc_obj.dst_ptr = (int*)ptr;
		G726_encode( &enc_obj );
		}
	///////////////////////////////////////////////////////////////////////////
	else if ( encoding_PT == 5 ) // G.726-16
	{
		int CFW = block_size * 2 / 16; // word count in coded frame
		unsigned short* ptr = out_pkt + 2 + cur_block * CFW;
		payload_size = pkt_size * CFW;

		enc_obj.src_ptr = rx_buf;
		enc_obj.dst_ptr = (int*)ptr;
		G726_encode( &enc_obj ); // srcBuf -> codeBuf
		}
	///////////////////////////////////////////////////////////////////////////
	else if ( encoding_PT == 6 || encoding_PT == 7 ) // G.729A
	{
		if ( cur_block == 0 || cur_block == 2 )
		{
			memcpy( rxFrameBuf, rx_buf, block_size * sizeof( int ) );
			}
		else
		{
			memcpy( rxFrameBuf + block_size, rx_buf, block_size * sizeof( int ) );

			static int codeWord[ G729AB_INIT_CWSIZE ];
			enc_obj7.src_ptr = rxFrameBuf;
			enc_obj7.dst_ptr = codeWord;
			
			int CWS = G729AB_encode( &enc_obj7 );
			
			if ( cur_block == 1 )
			{
				out_pkt[ 2 ] = CWS;
				if ( CWS )
					memcpy( out_pkt + 3, codeWord, CWS );
				}
			else // cur_block == 3
			{
				int lastCWS = out_pkt[ 2 ];
				out_pkt[ 2 ] = ( lastCWS << 8 ) | CWS;
				if ( CWS )
					memcpy( out_pkt + 3 + lastCWS, codeWord, CWS );

				payload_size = 1 + lastCWS + CWS;
				}
			}
		}

	if ( param.trace_pcm ) // signal trace
	{
		int* x = (int*)b_trace + 1 + cur_block * block_size * 2;
		for ( int i = 0; i < block_size; i++ )
		{
			*x++ = tx_buf[ i ] << 3;
			*x++ = rx_buf[ i ] << 3;	
			}
		}

	///////////////////////////////////////////////////////////////////////////
	// We should transmit outbound RTP packet after every pkt_size' blocks,
	// and also adjust Jitter Buffer settings.
	//
	if ( ++cur_block < pkt_size )
		return;

	cur_block = 0;
	++packets_count;
    ++localSeqNo;

	if ( param.trace_pcm )
	{
		b_trace[ 0 ] = ( ( id + 0x80 ) << 8 );
		outBuf.Put( MSGTYPE_IO_B, b_trace, 1 + pkt_size * block_size * 2 );
		}

	///////////////////////////////////////////////////////////////////////////
	// Send output packet
	// 
	if ( payload_size > 0 )
	{
	    out_pkt[ 0 ] = ( id << 8 ) | encoding_PT; // prefix packet with D channel ID
	    out_pkt[ 1 ] = localSeqNo;
	
	    outBuf.Put( MSGTYPE_IO_B, out_pkt, 2 + payload_size );
		}

	///////////////////////////////////////////////////////////////////////////
    // Adapt Jitter Buffer
    //
	++playRemoteSeqNo; // remote sequence number is incremental as the local one

	// Predict next remote sequence number based on statistics
	//
	unsigned short predRemoteSeqNo = 
		long( localSeqNo )
		+ ( ( jit_buf.RL_avg - jit_buf.JLen ) >> 4 ) / 160;

	// If predicted remote sequence number and locally incremented remote sequence
	// number are too much different, then accept predicted one as the
	//
	short delta = predRemoteSeqNo - playRemoteSeqNo;
	if ( delta < 0 )
	{
		// tracef( "delta -%x", -delta );
		playRemoteSeqNo = predRemoteSeqNo;
		++pred_adjusts_count;
		}
	else if ( delta > 1 )
	{
		// tracef( "delta %x", delta );
		playRemoteSeqNo = predRemoteSeqNo;
		++pred_adjusts_count;
		}

    curJitBuf = playRemoteSeqNo & ( JITBUF_MAXSIZE - 1 );

    ///////////////////////////////////////////////////////////////////////////
	// Change parameters on-fly, but still on the packet boundary.

	// Framer, first.
	//
	// TODO & Solve: Problems with different encoding / decoding framer characteristics

	// Encoder
	//
	if ( param.encoding_PT != encoding_PT )
		InitEncoder( param.encoding_PT );

	// Echo Canceller
	//
	if ( param.LEC_reinit != 0 ) // <> 0 if requested init
	{
		LEC_Init( param.LEC_reinit == 2 ); // 2 == warm start
		param.LEC_reinit = 0; // no reinit
		}
	}

///////////////////////////////////////////////////////////////////////////////
// Line Echo Canceller
//

///////////////////////////////////////////////////////////////////////////////
// LEC Initialization Function
//
void B_Channel::LEC_Init( bool warmstart )
{
    // Note: ec_obj.rin_ptr and .sin_ptr will be defined in LEC_Run

	GLOBAL_Params globals;
	globals.p0DBIN = param.p0DB; // Reference 0 dBm level for input signal
	globals.p0DBOUT = param.p0DB; // Reference 0 dBm level for output signal
	
    ///////////////////////////////////////////////////////////////////////////
    //  Initialize ECMR
    //
	if ( param.LEC_MinERL == 0 )
		ec_control = 0;
	else if ( param.LEC_MinERL == 3 ) 
		ec_control = ECMR_ERL1;
	else // default 6dB
		ec_control = ECMR_ERL2;
	
    if ( param.NLP_enabled )
     	ec_control |= ECMR_NLP;

    ec_obj.control = ec_control;
    ec_obj.nlpOut_ptr = nlpOutBuf;
    ec_obj.dlyBuf_ptr = delayBuf;

	ECMR_Params ecmrParams;
    ecmrParams.pBLOCKSIZE = block_size; // block size in samples
    ecmrParams.pMAXDELAY = param.LEC_TailLen; // PARAMETER: LEC maximum echo tail length
    ecmrParams.pISRBLKSIZE = 0; // ISR block size in sampled; 0 == disable ISR
    ecmrParams.pMINVOICEPOWER = param.NLP_MinVoicePower; // min power level for voice active
    ecmrParams.tVOICEHANGOVER = param.NLP_HangoverTime; // NLP hangover time

    ECMR_init( &ec_obj, &globals, &ecmrParams, 
    	warmstart ? ECMR_WARMSTART : ECMR_COLDSTART,
    	vpoStack_ptr );

    ///////////////////////////////////////////////////////////////////////////
    // Initialize NFE
    //
    nfe_obj.xmit_ptr = NULL;

	NFE_Params nfeParams;
    nfeParams.pMINNOISE = param.NFE_MinNoiseFloor; // minimum noise floor in dBm
    nfeParams.pMAXNOISE = param.NFE_MaxNoiseFloor; // maximum noise floor in dBm
    nfeParams.pBLKSIZE = block_size; // block size in samples

    NFE_init( &nfe_obj, &globals, &nfeParams, 
    	warmstart ? NFE_WARMSTART : NFE_COLDSTART,
    	vpoStack_ptr );

    ///////////////////////////////////////////////////////////////////////////
    // Initialize FMTD (2 instances)
    //
    fmtd_obj1.control = FMTD_ANSWER_WORD | FMTD_PHASE_WORD;

    FMTD_init( &fmtd_obj1, &globals, NULL, vpoStack_ptr );

    fmtd_obj2.control = FMTD_ANSWER_WORD | FMTD_PHASE_WORD;

    FMTD_init( &fmtd_obj2, &globals, NULL, vpoStack_ptr );
	}

///////////////////////////////////////////////////////////////////////////////
// CODEC Initialization Function
//
void B_Channel::InitEncoder( int payload )
{
	encoding_PT = payload;

	if ( encoding_PT == 0 ) // G.711 A-Law
	{
		// Nothing to do
		}
	else if ( encoding_PT == 1 ) // G.711 u-Law
	{
		// Nothing to do
		}
	else if ( encoding_PT == 2 ) // G.726-40
	{
	    G726_init( &enc_obj, G726_ENCODE, vpoStack_ptr, block_size, 5, 0 );
	    }
	else if ( encoding_PT == 3 ) // G.726-32
	{
	    G726_init( &enc_obj, G726_ENCODE, vpoStack_ptr, block_size, 4, 0 );
	    }
	else if ( encoding_PT == 4 ) // G.726-24
	{
	    G726_init( &enc_obj, G726_ENCODE, vpoStack_ptr, block_size, 3, 0 );
	    }
	else if ( encoding_PT == 5 ) // G.726-16
	{
	    G726_init( &enc_obj, G726_ENCODE, vpoStack_ptr, block_size, 2, 0 );
	    }
	else if ( encoding_PT == 6 ) // G.729A
	{
	    enc_obj7.vad_enable = 0; // Disable VAD
	    G729AB_init( &enc_obj7, G729AB_ENCODE, vpoStack_ptr, g729_B1mem );
		}
	else if ( encoding_PT == 7 ) // G.729A+B
	{
	    enc_obj7.vad_enable = 1; // Enable Annex B: VAD/CNG
	    G729AB_init( &enc_obj7, G729AB_ENCODE, vpoStack_ptr, g729_B1mem );
		}
	}

///////////////////////////////////////////////////////////////////////////////

void B_Channel::StartTransmission( void )
{
	cur_block = 0;
	cur_frame = 0;
	io_buf.ClearTxBoth (); // Clear double buffer output samples

	curJitBuf = -1;
	pred_adjusts_count = 0;
	tx_depleted_count = 0;
	packets_count = 0;
	jit_buf.ClearAll ();

	LEC_Init( param.LEC_WarmStart );

	InitEncoder( param.encoding_PT ); // keep previous value

	inSpeech = true;
/*
	if ( id == 0 && ! D_Ch->dasl.IsMaster () )
		io_buf.SetLoopback( true );
*/		
	}

void B_Channel::StopTransmission( void )
{
	inSpeech = false;
	}

///////////////////////////////////////////////////////////////////////////////

void B_Channel::ChangeParameters( unsigned char* data, int data_len )
{
	if ( data_len < 1 )
		return;

	tracef( "%c: B Set: %a", id, data, data_len );

	switch( data[ 0 ] )
	{
		case 0x00: // Query settings

			tracef( "%c: ----------------------------------" );
			tracef( "%c: General: 0dB=%x, BSZ=%c ISR=%c PSZ=%x, LOP=%c, TRC=%c", id, param.p0DB, param.block_size, param.ISR_block_size, param.packet_size, io_buf.IsLoopback (), param.trace_pcm );
			tracef( "%c: CODEC: PT=%c, McBSP=%c", id, param.encoding_PT, param.alaw_companding );
			tracef( "%c: Jitter: Min=%c, Max=%c, A=%c, B=%c", id, jit_buf.JLenMin, jit_buf.JLenMax, jit_buf.JAlpha, jit_buf.JBeta );
			tracef( "%c: LEC: On=%c, WS=%c, Len=%x, ERL=%c", id, param.LEC_enabled, param.LEC_WarmStart, param.LEC_TailLen, param.LEC_MinERL );
			tracef( "%c: NFE: Min=%x, Max=%x", id, param.NFE_MinNoiseFloor, param.NFE_MaxNoiseFloor );
			tracef( "%c: NLP: On=%c, MVP=%x, HT=%x", id, param.NLP_enabled, param.NLP_MinVoicePower, param.NLP_HangoverTime );
			tracef( "%c: FMTD: On=%c", id, param.FMTD_enabled );
			break;

		case 0x01: // Set CODEC Encoder Payload Type

			if ( data_len < 2 )
				break;

			param.encoding_PT = data[ 1 ];
			
			if ( param.encoding_PT < 0 )
				param.encoding_PT = 0;
			else if ( param.encoding_PT > 7 && param.encoding_PT < 250 )
				param.encoding_PT = 0;
				
			if ( data_len >= 5 )
			{
				param.compressed_RTP = data[ 2 ];
				param.packet_size = ( data[ 3 ] << 8 ) | data[ 4 ];
				}

			if ( data_len >= 6 )
			{
				io_buf.SetLoopback( data[ 5 ] );
				}

			break;

		case 0x02: // Set Jitter Buffer Parameters; immediate change

			if ( data_len < 7 )
				break;

			jit_buf.JLenMin = ( ( data[ 1 ] << 8 ) | data[ 2 ] );
			jit_buf.JLenMax = ( ( data[ 3 ] << 8 ) | data[ 4 ] );
			jit_buf.JAlpha = data[ 5 ];
			jit_buf.JBeta = data[ 6 ];

			// Sanity Check
			//
			if ( jit_buf.JLenMin < 0 )
				jit_buf.JLenMin = 0;
			else if ( jit_buf.JLenMax > 200 )
				jit_buf.JLenMax = 200;
				
			if ( jit_buf.JLenMax < 0 )
				jit_buf.JLenMax = 0;
			else if ( jit_buf.JLenMax > 200 )
				jit_buf.JLenMax = 200;
				
			if ( jit_buf.JLenMin > jit_buf.JLenMax )
				jit_buf.JLenMax = jit_buf.JLenMin;

			if ( jit_buf.JAlpha < 2 )
				jit_buf.JAlpha = 2;
			else if ( jit_buf.JAlpha > 12 )
				jit_buf.JAlpha = 12;
				
			if ( jit_buf.JBeta < 1 )
				jit_buf.JBeta = 1;
			else if ( jit_buf.JBeta > 8 )
				jit_buf.JBeta = 8;

			break;

		case 0x03: // Set Echo Canceller Parameters

			if ( data_len < 21 )
				break;

			param.LEC_reinit = data[ 1 ] ? 2 : 1; // warm -> 2, cold -> 1
			param.p0DB = ( data[ 2 ] << 8 ) | data[ 3 ];
			param.block_size = data[ 4 ];
			param.ISR_block_size = data[ 5 ];
			param.LEC_enabled = data[ 6 ];
			param.LEC_TailLen = ( ( data[ 7 ] << 8 ) | data[ 8 ] ) * 8;
			param.LEC_WarmStart = data[ 9 ];
			param.LEC_MinERL = data[ 10 ];
			param.NFE_MinNoiseFloor = ( data[ 11 ] << 8 ) | data[ 12 ];
			param.NFE_MaxNoiseFloor = ( data[ 13 ] << 8 ) | data[ 14 ];
			param.NLP_enabled = data[ 15 ];
			param.NLP_MinVoicePower = ( data[ 16 ] << 8 ) | data[ 17 ];
			param.NLP_HangoverTime = ( data[ 18 ] << 8 ) | data[ 19 ];
			param.FMTD_enabled = data[ 20 ];

			// Sanity check
			//
			if ( param.p0DB < 1000 )
				param.p0DB = 1000;
			else if ( param.p0DB > 8000 )
				param.p0DB = 8000;

			if ( param.block_size < 40 )
				param.block_size = 40;
			else if ( param.block_size > 80 )
				param.block_size = 80;
			
			param.block_size &= ~0x3; // must be divisible by 4 (0.5ms grain)
			
			if ( param.LEC_TailLen < 256 ) param.LEC_TailLen = 256;
			else if ( param.LEC_TailLen > 1024 ) param.LEC_TailLen = 1024;

			param.LEC_TailLen &= ~0x7F; // must be dibisible by 128 (16ms grain)

			if ( param.NFE_MinNoiseFloor < -62 )
				param.NFE_MinNoiseFloor = -62;
			else if ( param.NFE_MinNoiseFloor > -22 )
				param.NFE_MinNoiseFloor = -22;
				
			if ( param.NFE_MaxNoiseFloor < -62 )
				param.NFE_MaxNoiseFloor = -62;
			else if ( param.NFE_MaxNoiseFloor > -22 )
				param.NFE_MaxNoiseFloor = -22;
				
			if ( param.NFE_MinNoiseFloor > param.NFE_MaxNoiseFloor )
			{
				param.NFE_MinNoiseFloor = NFE_MINNOISE_DEF;
				param.NFE_MaxNoiseFloor = NFE_MAXNOISE_DEF;
				}

			if ( param.NLP_MinVoicePower != 0 )
			{
				if ( param.NLP_MinVoicePower < ECMR_MIN_VOICE_PWR_MIN )
					param.NLP_MinVoicePower = ECMR_MIN_VOICE_PWR_MIN;
				else if ( param.NLP_MinVoicePower > ECMR_MIN_VOICE_PWR_MAX )
					param.NLP_MinVoicePower = ECMR_MIN_VOICE_PWR_MAX;
				}

			if ( param.NLP_HangoverTime < ECMR_VOICE_HNG_CNT_MIN )
				param.NLP_HangoverTime = ECMR_VOICE_HNG_CNT_MIN;
			else if ( param.NLP_HangoverTime > ECMR_VOICE_HNG_CNT_MAX )
				param.NLP_HangoverTime = ECMR_VOICE_HNG_CNT_MAX;

			break;
			
		case 0x04: // Set PCM trace on/off
			
			if ( data_len < 2 )
				break;

			param.trace_pcm = data[ 1 ];
			
			break;
		}
	}

///////////////////////////////////////////////////////////////////////////////

void B_JitterBuffer::Put( unsigned long L_ts, unsigned char* pkt, int pkt_len )
{
	++packets_count;

	// Parse Header (MSB first):
	//
	// payload : uint8
	// sequence number : uint16
	//
	unsigned short seq_no = ( pkt[ 1 ] << 8 ) | pkt[ 2 ];
	int ix = seq_no & ( JITBUF_MAXSIZE - 1); // the same as % JITBUF_MAXSIZE
	payload[ ix ] = pkt[ 0 ];

	seqNo[ ix ] = seq_no;

	// Put packed octets into words, LSB first. 
	// Payload length must be multiple of encoded blk_size
	// Note:
	// - data[] comes in unpacked words
	// - len is in unpacked words i.e. in octets.
	//
	unsigned short* jb = codew[ ix ]; // destination
	unsigned char* data = pkt + 3; // source
	int len = pkt_len - 3;
	if ( len > 160 )
		len = 160; // Max 160 octets; Suppress Overflow

	for ( int i = 0; i < len; i += 2 )
	{
		*jb++ = ( data[ i ] << 8 ) | data[ i + 1 ];
		}
		
	// Jitter Buffer Statistics
	//
	unsigned long R_ts = (unsigned long)seq_no * 160;
	long t = long( R_ts - L_ts ) << 4;
	long v = t - RL_avg;
	long abs_v = v > 0 ? v : -v;

	int alpha = JAlpha;
	if ( abs_v > ( ( 10L * 160 ) << 4 ) ) // if abs_v > 200 ms
	{
		alpha = 0; // rapid follow up
		abs_v = 0;
		}
	else if ( abs_v > ( ( 5L * 160 ) << 4 ) ) // if abs_v > 100 ms
	{
		alpha = 1; // rapid follow up
		}

	RL_avg += ( v >> alpha );
	RL_var += ( ( abs_v - RL_var ) >> alpha );

	JLen = JBeta * RL_var + ( 80 << 4 );

	long bound = ( long( JLenMin ) * 160 / 20 ) << 4;
	if ( JLen < bound ) JLen = bound;

	bound = ( long( JLenMax ) * 160 / 20 ) << 4;
	if ( JLen > bound ) JLen = bound;
/*
	tracef( "%x %x, %x %x, %x", 
		(unsigned int)( t >> 4 ), (unsigned int)( RL_avg >> 4 ),
		(unsigned int)( abs_v >> 4 ), (unsigned int)( RL_var >> 4 ), 
		alpha );
*/		
	}
