

#if 0
inline int s13_to_alaw ( short pcm_val )
{
	// Get sign & magnitude
	//
    int sign = pcm_val < 0 ? 0x80 : 0x00; // sign
    if ( pcm_val < 0 )
        pcm_val = -pcm_val; // magnitude
        
	// Assert: pcm_val msut be <= 0xFFF !

    // Convert the scaled magnitude to chord number.
    //
    static const int chord_end[ 8 ] =
    {
        0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF
    	};
    int chord = 0;
    while ( chord < 7 && pcm_val > chord_end[ chord ] )
    	++ chord;

    // Combine the sign, segment, and quantization bits.
    //
    int step = ( pcm_val >> ( chord == 0 ? 1 : chord ) ) & 0x0F;

    // printf( "[%04hx, %02x, %d, %04hx]\n", pcm_val, sign, chord, step );
    
    return 0x55 ^ ( sign | ( chord << 4 ) | step );
	}
	
inline short alaw_to_s13( int a_val )
{
	a_val ^= 0x55;
	
	int sign = a_val & 0x80;
	int chord = ( a_val >> 4 ) & 0x07;
	
	short step = ( ( a_val & 0x0F ) << 1 ) + 1;
	if ( chord > 0 )
		step |= 0x20;
	if ( chord > 1 )
		step <<= ( chord - 1 );
	
	return ( sign ? -step : step );
	}

#endif
///////////////////////////////////////////////////////////////////////////////

#if 0
		extern volatile short B0_c;
		extern volatile short B0_rx;
		extern volatile short B0_tx;
		
		if ( B0_c >= 2 )
		{
			B0_c = 0;
			
			if ( 1 || DASL_Config == DASL::MASTER ) // EXTENDER: Just s13<->alaw
			{
		    	B0_tx = ( (unsigned short)B0.xmt_buf.Get () << 8 ) | 0x00FF;
		    	B0.rcv_buf.Put( (unsigned short)B0_rx >> 8 );
		    	}
		    else // GATEWAY: LEC & s13<->alaw
		    {
/*
                // Loopback:
                //
				B0_tx = B0_rx;
		    	B0.rcv_buf.Put( B0.xmt_buf.Get () );
*/		    	
		    	int ref_alaw = B0.xmt_buf.Get ();
		    	B0_tx = ( (unsigned short)ref_alaw << 8 ) | 0x00FF;
		    	
	    		B0.rcv_buf.Put( 
	    			s13_to_alaw( 
	    				LEC_input(
    						alaw_to_s13( ref_alaw ) << 3, // reference
    						alaw_to_s13( (unsigned short)B0_rx >> 8 ) << 3 // near end
	    					) >> 3 
	    				) 
	    			);
	    		}
			}
#endif	    			

