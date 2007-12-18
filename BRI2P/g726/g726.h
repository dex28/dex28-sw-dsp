//
// g726.h
//
// Header file for ITU-T G.726 recommendation conversion routines.
//
//
#ifndef _G726_H
#define _G726_H

// #include "../g711.h"
extern unsigned char linear2alaw( int pcm_val );
extern int alaw2linear( unsigned char a_val );

extern unsigned char linear2ulaw( int pcm_val );
extern int ulaw2linear( unsigned char a_val );

static inline int abs( int x ) { return x < 0 ? -x : x; }

enum
{
    AUDIO_ENCODING_ULAW    = 1, // ITU-T G.711 u-law
    AUDIO_ENCODING_ALAW    = 2, // ITU-T G.711 A-law
    AUDIO_ENCODING_LINEAR  = 3  // PCM 2's-complement (0-center)
    };

// The clas comprise an implementation of the ITU-T G.726 ADPCM
// coding algorithm. Essentially, this implementation is identical to
// the bit level description except for a few deviations which
// take advantage of work station attributes, such as hardware 2's
// complement arithmetic and large memory. Specifically, certain time
// consuming operations such as multiplications are replaced
// with lookup tables and software 2's complement operations are
// replaced with hardware 2's complement.
//
// The deviation from the bit level specification (lookup tables)
// preserves the bit level performance specifications.
//
// As outlined in the G.721 Recommendation, the algorithm is broken
// down into modules.  Each section of code below is preceded by
// the name of the module which it is implementing.
//
// The ITU-T G.726 coder is an adaptive differential pulse code modulation
// (ADPCM) waveform coding algorithm, suitable for coding of digitized
// telephone bandwidth (0.3-3.4 kHz) speech or audio signals sampled at 8 kHz.
// This coder operates on a sample-by-sample basis. Input samples may be 
// represented in linear PCM or companded 8-bit G.711 (m-law/A-law) formats
// (i.e., 64 kbps). For 32 kbps operation, each sample is converted into a
// 4-bit quantized difference signal resulting in a compression ratio of 
// 2:1 over the G.711 format. For 24 kbps 40 kbps operation, the quantized
// difference signal is 3 bits and 5 bits, respectively.
//
class g726_state
{
private:

    // The following is the definition of the state structure
    // used by the G.721/G.726 encoder and decoder to preserve their internal
    // state between successive calls. The meanings of the majority
    // of the state structure fields are explained in detail in the
    // ITU-T Recommendation G.726. The field names are essentially indentical
    // to variable names in the bit level description of the coding algorithm
    // included in this Recommendation.

    long    YL;         // Locked or steady state step size multiplier.

    int     YU;         // Unlocked or non-steady state step size multiplier.

    int     DMS;        // Short term energy estimate.

    int     DML;        // Long term energy estimate.

    int     AP;         // Linear weighting coefficient of 'YL' and 'YU'.

    int     A  [ 2 ];   // Coefficients of pole portion of prediction filter.

    int     B  [ 6 ];   // Coefficients of zero portion of prediction filter.

    int     PK [ 2 ];   // Signs of previous two samples of a partially
                        // reconstructed signal.

    short   DQ [ 6 ];   // Previous 6 samples of the quantized difference
                        // signal represented in an internal Q3.6 floating point
                        // format.

    short   SR [ 2 ];   // Previous 2 samples of the quantized difference
                        // signal represented in an internal Q3.6 floating point
                        // format.

    int     TD;         // delayed tone detect, new in 1988 version

private:

    static int power2[ 15 ];

    // search
    //
    // Quantizes the input value against the table of integers.
    // It returns 'i', if table[i-1] <= val < table[i].
    //
    // Uses linear search for simple coding.
    //
    static inline int
    search
    (
        int value,
        int table [],
        int size
        )
    {
        for ( int i = 0; i < size; i++ )
            if ( value < *table++ )
                return i;

        return size;
        }

    // fmult
    //
    // Returns the integer product of the 14-bit integer "an" and
    // "floating point" representation (4-bit exponent, 6-bit mantessa) "srn".
    // 'an' is accepted as 16-bit integer and divided by 4 before multiplication.
    //
    static inline int
    fmult
    (
        int an, // 16-bit integer
        int srn // floating point: sign, 4-bit exponent, 6-bit mantissa
        )
    {
        an >>= 2; // make 'an' to be 14-bit integer

        int anmag = ( an > 0 ) ? an : ( -an ) & 0x1FFF;
        int anexp = search( anmag, power2, 15 ) - 6;
        int anmant = ( anmag == 0 ) ? 32 
            : ( anexp >= 0 ) ? anmag >> anexp : anmag << -anexp;
        int wanexp = anexp + ( ( srn >> 6 ) & 0xF) - 13;

        int wanmant = ( anmant * ( srn & 0x3F ) + 0x30 ) >> 4;
        int retval = ( wanexp >= 0 ) ? ( wanmant << wanexp ) & 0x7FFF 
            : wanmant >> -wanexp;

        return ( an ^ srn ) < 0 ? -retval : retval;
        }

    //
    // predictor_zero
    //
    // computes the estimated signal from 6-zero predictor.
    //
    inline int predictor_zero( void )
    {
        int sezi = 0;

        for ( int i = 0; i < 6; i++ ) // ACCUM
            sezi += fmult( B[i], DQ[i] );

        return sezi;
        }

    //
    // predictor_pole
    //
    // Computes the estimated signal from 2-pole predictor.
    //
    inline int predictor_pole( void )
    {
        return fmult( A[1], SR[1] ) 
             + fmult( A[0], SR[0] );
    }

    //
    // step_size
    //
    // Computes the quantization step size of the adaptive quantizer.
    //
    inline int step_size( void )
    {
        if ( AP >= 256 )
            return YU;

        int y = YL >> 6;
        int dif = YU - y;
        int al = AP >> 2;
        if ( dif > 0 )
            y += ( dif * al ) >> 6;
        else if ( dif < 0 )
            y += ( dif * al + 0x3F ) >> 6;

        return y;
        }

    // update
    //
    // Updates the state variables for each output code.
    //
    void update
    (
        int code_size,  // distinguish 726_40 with others
        int y,          // quantizer step size
        int wi,         // scale factor multiplier
        int fi,         // for long/short term energies
        int dq,         // quantized prediction difference
        int sr,         // reconstructed signal
        int dqsez       // difference from 2-pole predictor
        );

    // quantize
    //
    // Given a raw sample, 'd', of the difference signal and a
    // quantization step size scale factor, 'y', this routine returns the
    // ADPCM codeword to which that sample gets quantized.  The step
    // size scale factor division operation is done in the log base 2 domain
    // as a subtraction.
    //
    static inline int
    quantize
    (
        int d,          // Raw difference signal sample
        int y,          // Step size multiplier
        int table [],   // quantization table of integers
        int size        // table size
        )
    {
        //
        // LOG
        //
        // Compute base 2 log of 'd', and store in 'dl'.
        //    
        int dqm = abs( d );                         // Maginuted of 'd' 
        int exp = search( dqm >> 1, power2, 15 );   // Integer part of base 2 log of 'd'
        int mant = ( ( dqm << 7 ) >> exp ) & 0x7F;  // Fractional part of base 2 log
        int dl = ( exp << 7 ) + mant;               // Log of magniuted of 'd'

        //
        // SUBTB
        //
        // "Divide" by step size multiplier.
        //
        int dln = dl - ( y >> 2 );                  // Step size scale factor nomalized log

        //
        // QUAN
        //
        // Obtain codword 'i' for 'd'.
        //
        int i = search( dln, table, size );
        if ( d < 0 )                                // take 1's complement of i
            return ( size << 1 ) + 1 - i;
        if ( i == 0 )                               // take 1's complement of 0
            return ( size << 1 ) + 1;               // new in 1988
        return i;
        }

    // reconstruct
    //
    // Returns reconstructed difference signal 'dq' obtained from
    // codeword 'i' and quantization step size scale factor 'y'.
    // Multiplication is performed in log base 2 domain as addition.
    //
    static inline int
    reconstruct
    (
        int sign,   // 0 for non-negative value
        int dqln,   // G.726 codeword
        int y       // Step size multiplier
        )
    {
        int dql = dqln + (y >> 2); // Log of 'dq' magnitude  // ADDA

        if ( dql < 0 )
            return sign ? -0x8000 : 0;

        // ANTILOG
        int dex = ( dql >> 7 ) & 15; // Integer part of log
        int dqt = 128 + ( dql & 127 );
        int dq = short( ( dqt << 7 ) >> ( 14 - dex ) ); // Reconstructed difference signal sample
        return sign ? ( dq - 0x8000 ) : dq;
    }

    // tandem_adjust_*
    //
    // At the end of ADPCM decoding, it simulates an encoder which may be receiving
    // the output of this decoder as a tandem process. If the output of the
    // simulated encoder differs from the input to this decoder, the decoder output
    // is adjusted by one level of A-law or u-law codes.
    //
    // Input:
    //  sr  decoder output linear PCM sample,
    //  se  predictor estimate sample,
    //  y   quantizer step size,
    //  i   decoder input code,
    //  sign    sign bit of code i
    //
    // Return:
    //  adjusted A-law or u-law compressed sample.
    //
    static int tandem_adjust_alaw
    (
        int sr,         // decoder output linear PCM sample
        int se,         // predictor estimate sample
        int y,          // quantizer step size
        int i,          // decoder input code
        int sign,
        int qtab []
        );

    static int tandem_adjust_ulaw
    (
        int sr,         // decoder output linear PCM sample
        int se,         // predictor estimate sample
        int y,          // quantizer step size
        int i,          // decoder input code
        int sign,
        int qtab []
        );

public:

    // Constructor
    // g726_state::g726_state()
    //
    // This routine initializes and/or resets the g726_state structure.
    // All the initial state values are specified in the ITU-T G.726 document.
    //
    g726_state( void )
    {
        YL = 34816;
        YU = 544;
        DMS = 0;
        DML = 0;
        AP = 0;

        for ( int i = 0; i < 2; i++ )
        {
            A[i] = 0;
            PK[i] = 0;
            SR[i] = 32;
        }

        for ( int i = 0; i < 6; i++ )
        {
            B[i] = 0;
            DQ[i] = 32;
        }

        TD = 0;
        }


    // Methods

    // encoder_*
    //
    // Encodes the input vale of linear PCM, A-law or u-law data sl and returns
    // the resulting code. -1 is returned for unknown input coding value.
    //
    // decoder_*
    //
    // Decodes a 4-bit code of G.721 encoded data of i and
    // returns the resulting linear PCM, A-law or u-law value.
    // return -1 for unknown out_coding value.
    //
    int encoder_16(
        int sample,
        int in_coding
        );

    int decoder_16(
        int code,
        int out_coding
        );

    int encoder_24(
        int sample,
        int in_coding
        );

    int decoder_24(
        int code,
        int out_coding
        );

    int encoder_32(
        int sample,
        int in_coding
        );

    int decoder_32(
        int code,
        int out_coding
        );

    int encoder_40(
        int sample,
        int in_coding
        );

    int decoder_40(
        int code,
        int out_coding
        );
    };

#endif // _G726_H included
