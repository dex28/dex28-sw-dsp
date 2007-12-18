#include "g726.h"

int g726_state::power2[ 15 ] = 
{
    0x0001, 0x0002, 0x0004, 0x0008, 
    0x0010, 0x0020, 0x0040, 0x0080,
    0x0100, 0x0200, 0x0400, 0x0800,
    0x1000, 0x2000, 0x4000
    };

//
// Common routines for G.721 and G.726 conversions.
//

void
g726_state::update
(
    int code_size,  // distinguish 726_40 with others
    int y,          // quantizer step size
    int wi,         // scale factor multiplier
    int fi,         // for long/short term energies
    int dq,         // quantized prediction difference
    int sr,         // reconstructed signal
    int dqsez       // difference from 2-pole predictor
    )
{
    int pk0 = ( dqsez < 0 ) ? 1 : 0;  // needed in updating predictor poles

    int mag = dq & 0x7FFF;            // prediction difference magnitude; FLOAT A

    // TRANS
    //
    int tr = 0; // tone/transition detector
    if ( TD != 0 ) // if supposed data
    {
        int ylint = YL >> 15;               // exponent part of yl
        int ylfrac = ( YL >> 10 ) & 0x1F;   // fractional part of yl
        int thr1 = (32 + ylfrac) << ylint;      // threshold
        int thr2 = (ylint > 9) ? 31 << 10 : thr1;   // limit thr2 to 31 << 10
        int dqthr = (thr2 + (thr2 >> 1)) >> 1;  // dqthr = 0.75 * thr2

        tr = ( mag <= dqthr ) // supposed data, but small mag
            ? 0  // signal is voice
            : 1; // signal is data (modem)
        }

    //
    // Quantizer scale factor adaptation.
    //

    // FUNCTW & FILTD & DELAY
    // update non-steady state step size multiplier
    //
    YU = y + ( ( wi - y ) >> 5 );

    // LIMB
    //
    if ( YU < 544 ) // 544 <= yu <= 5120
        YU = 544;
    else if ( YU > 5120 )
        YU = 5120;

    // FILTE & DELAY
    // update steady state step size multiplier
    //
    YL += YU + ( ( -YL ) >> 6 );

    //
    // Adaptive predictor coefficients.
    //
    int a2p; // LIMC

    if ( tr == 1 )
    {
        // Reset a's and b's for modem signal
        A[0] = 0;  A[1] = 0;
        B[0] = 0;  B[1] = 0;  B[2] = 0;  B[3] = 0;  B[4] = 0;  B[5] = 0;
        a2p = 0;
        }
    else
    {
        // update a's and b's

        int pks1 = pk0 ^ PK[0]; // UPA2

        // update predictor pole a[1]

        a2p = A[1] - ( A[1] >> 7 );

        if ( dqsez != 0 )
        {
            int fa1 = (pks1) ? A[0] : -A[0];
            if (fa1 < -8191)    // a2p = function of fa1
                a2p -= 0x100;
            else if (fa1 > 8191)
                a2p += 0xFF;
            else
                a2p += fa1 >> 5;

            if (pk0 ^ PK[1])
            {
                // LIMC
                if (a2p <= -12160)
                    a2p = -12288;
                else if (a2p >= 12416)
                    a2p = 12288;
                else
                    a2p -= 0x80;
                }
            else if (a2p <= -12416)
                a2p = -12288;
            else if (a2p >= 12160)
                a2p = 12288;
            else
                a2p += 0x80;
        }

        // TRIGB & DELAY
        A[1] = a2p;

        // UPA1
        // update predictor pole a[0]
        A[0] -= A[0] >> 8;
        if (dqsez != 0)
            if (pks1 == 0)
                A[0] += 192;
            else
                A[0] -= 192;

        // LIMD
        int a1ul = 15360 - a2p; // UPA1
        if (A[0] < -a1ul)
            A[0] = -a1ul;
        else if (A[0] > a1ul)
            A[0] = a1ul;

        // UPB : update predictor zeros b[6]
        for ( int i = 0; i < 6; i++ ) 
        {
            if ( code_size == 5 )     // for 40kbps G.726
                B[i] -= B[i] >> 9;
            else                      // for 16, 24 & 32kbps G.726
                B[i] -= B[i] >> 8;

            if ( dq & 0x7FFF )
            {
                // XOR
                if ( ( dq ^ DQ[i] ) >= 0 )
                    B[i] += 128;
                else
                    B[i] -= 128;
                }
            }
        }

    for ( int i = 5; i > 0; i-- )
        DQ[i] = DQ[i-1];

    // FLOAT A : convert dq[0] to 4-bit exp, 6-bit mantissa f.p.
    if ( mag == 0 )
    {
        DQ[0] = (dq >= 0) ? 0x20 : short( 0xFC20 );
        }
    else 
    {
        int exp = search( mag, power2, 15 );
        DQ[0] = short( (dq >= 0) ?
            (exp << 6) + ((mag << 6) >> exp) :
            (exp << 6) + ((mag << 6) >> exp) - 0x400 );
        }

    SR[1] = SR[0];

    // FLOAT B : convert sr to 4-bit exp., 6-bit mantissa f.p.

    if (sr == 0) 
    {
        SR[0] = 0x20;
        }
    else if (sr > 0)
    {
        int exp = search( sr, power2, 15 );
        SR[0] = (exp << 6) + ((sr << 6) >> exp);
        }
    else if (sr > -32768)
    {
        mag = -sr;
        int exp = search( mag, power2, 15 );
        SR[0] =  (exp << 6) + ((mag << 6) >> exp) - 0x400;
        }
    else
        SR[0] = short( 0xFC20 );

    // DELAY A
    PK[1] = PK[0];
    PK[0] = pk0;

    // TONE
    if ( tr == 1 )              // this sample has been treated as data
        TD = 0;                 // next one will be treated as voice
    else if ( a2p < -11776 )    // small sample-to-sample correlation
        TD = 1;                 // signal may be data
    else                        // signal is voice
        TD = 0;

    //
    // Adaptation speed control.
    //    
    DMS += ( fi - DMS ) >> 5;               // FILTA
    DML += ( (  ( fi << 2 ) - DML ) >> 7 ); // FILTB

    if ( tr == 1 )
        AP = 256;
    else if ( y < 1536 )                    // SUBTC
        AP += (0x200 - AP) >> 4;
    else if ( TD == 1 )
        AP += ( 0x200 - AP ) >> 4;
    else if ( abs( ( DMS << 2 ) - DML ) >= ( DML >> 3 ) )
        AP += ( 0x200 - AP ) >> 4;
    else
        AP += (-AP) >> 4;
    }

int
g726_state::tandem_adjust_alaw
(
    int sr, // decoder output linear PCM sample
    int se, // predictor estimate sample
    int y,  // quantizer step size
    int i,  // decoder input code
    int sign,
    int qtab []
    )
{

    if ( sr <= -32768 )
        sr = -1;

    int sp = linear2alaw( ( sr >> 1 ) << 3 );   // A-law compressed 8-bit code
    int dx = ( alaw2linear( sp ) >> 2 ) - se;   // 16-bit prediction error
    int id = quantize( dx, y, qtab, sign - 1 ); // quantized prediction error

    if ( id == i ) // no adjustment on sp
        return sp;

    // sp adjustment needed

    // ADPCM codes : 8, 9, ... F, 0, 1, ... , 6, 7
    int im = i ^ sign;   // 2's complement to biased unsigned magnitude of i
    int imx = id ^ sign; // biased magnitude of id

    int sd; // adjusted A-law decoded sample value

    if ( imx > im ) 
    {
        // sp adjusted to next lower value
        if ( sp & 0x80 )
            sd = (sp == 0xD5) ? 0x55 : ((sp ^ 0x55) - 1) ^ 0x55;
        else
            sd = (sp == 0x2A) ? 0x2A : ((sp ^ 0x55) + 1) ^ 0x55;
        } 
    else 
    {
        // sp adjusted to next higher value
        if ( sp & 0x80 )
            sd = (sp == 0xAA) ? 0xAA : ((sp ^ 0x55) + 1) ^ 0x55;
        else
            sd = (sp == 0x55) ? 0xD5 : ((sp ^ 0x55) - 1) ^ 0x55;
        }

    return sd;
    }

int
g726_state::tandem_adjust_ulaw
(
    int sr, // decoder output linear PCM sample
    int se, // predictor estimate sample
    int y,  // quantizer step size
    int i,  // decoder input code
    int sign,
    int qtab []
    )
{

    if ( sr <= -32768 )
        sr = 0;

    int sp = linear2ulaw(sr << 2);  // short to u-law compression; u-Law compressed 8-bit code
    int dx = (ulaw2linear(sp) >> 2) - se;   // 16-bit prediction error
    int id = quantize(dx, y, qtab, sign - 1); // quantized prediction error

    if (id == i)
        return sp;

    // sp adjustment needed

    int sd; // adjusted u-law decoded sample value

    // ADPCM codes : 8, 9, ... F, 0, 1, ... , 6, 7
    int im = i ^ sign;      // 2's complement to biased unsigned magnitued of i
    int imx = id ^ sign; // biased magnitude of id
    if (imx > im)
    {
        // sp adjusted to next lower value
        if (sp & 0x80)
            sd = (sp == 0xFF) ? 0x7E : sp + 1;
        else
            sd = (sp == 0) ? 0 : sp - 1;

        } 
    else 
    {
        // sp adjusted to next higher value
        if ( sp & 0x80 )
            sd = (sp == 0x80) ? 0x80 : sp - 1;
        else
            sd = (sp == 0x7F) ? 0xFE : sp + 1;
        }

    return sd;
    }

///////////////////////////////////////////////////////////////////////////////
// G.726 ADPCM 32kbps implementation

static int q_tab_32[7] = 
{
    -124, 80, 178, 246, 300, 349, 400
    };

// Maps G.726 code word to reconstructed scale factor normalized log
// magnitude values.
//
static short dqln_tab_32[16] = 
{
    -2048, 4, 135, 213, 273, 323, 373, 425,
    425, 373, 323, 273, 213, 135, 4, -2048
    };

// Maps G.726 code word to log of scale factor multiplier.
//
static short wi_tab_32[16] = 
{
    -12, 18, 41, 64, 112, 198, 355, 1122,
    1122, 355, 198, 112, 64, 41, 18, -12
    };

// Maps G.726 code words to a set of values whose long and short
// term averages are computed and then compared to give an indication
// how stationary (steady state) the signal is.
//
static short fi_tab_32[16] = 
{
    0, 0, 0, 0x200, 0x200, 0x200, 0x600, 0xE00,
    0xE00, 0x600, 0x200, 0x200, 0x200, 0, 0, 0
    };

int
g726_state::encoder_32
(
    int sl,         // sample
    int in_coding   // coding type of the sample
    )
{
    // Linearize input sample to 16-bit PCM
    //
    switch ( in_coding )
    {    
        case AUDIO_ENCODING_ALAW:
            sl = alaw2linear( sl );
            break;
        case AUDIO_ENCODING_ULAW:
            sl = ulaw2linear( sl );
            break;
        case AUDIO_ENCODING_LINEAR:
            break;
        default:
            return -1;
        }

    // Force 14-bit sample dynamic range
    //
    sl >>= 2;

    int sezi = predictor_zero ();
    int sez = sezi >> 1;
    int sei = sezi + predictor_pole ();
    int se = sei >> 1;  // estimated signal

    int d = sl - se;                // estimation difference; SUBTA

    // quantize the prediction difference
    int y = step_size ();       // quantizer step size; MIX

    int i = quantize(d, y, q_tab_32, 7);    // i = ADPCM code

    int dq = reconstruct( i & 0x08, dqln_tab_32[i], y );    // quantized est diff

    int sr = ( dq < 0 ) ? se - ( dq & 0x3FFF ) : se + dq;   // reconst. signal; ADDB

    int dqsez = sr + sez - se;          // pole prediction diff; ADDC

    update( 4, y, wi_tab_32[i] << 5, fi_tab_32[i], dq, sr, dqsez );

    return i;
    }

int
g726_state::decoder_32
(
    int i,
    int out_coding
    )
{
    i &= 0x0f;              // mask to get proper bits

    int sezi = predictor_zero ();
    int sez = sezi >> 1;
    int sei = sezi + predictor_pole ();
    int se = sei >> 1;          // se = estimated signal

    int y = step_size ();   // dynamic quantizer step size; MIX

    int dq = reconstruct( i & 0x08, dqln_tab_32[i], y ); // quantized diff.

    int sr = (dq < 0) ? (se - (dq & 0x3FFF)) : se + dq; // reconst. signal; ADDB

    int dqsez = sr + sez - se;          // pole prediction diff.

    update( 4, y, wi_tab_32[i] << 5, fi_tab_32[i], dq, sr, dqsez );

    switch (out_coding) 
    {
        case AUDIO_ENCODING_ALAW:
            return tandem_adjust_alaw( sr, se, y, i, 8, q_tab_32 );

        case AUDIO_ENCODING_ULAW:
            return tandem_adjust_ulaw( sr, se, y, i, 8, q_tab_32 );

        case AUDIO_ENCODING_LINEAR:
            {
                long lino = long( sr ) << 2;  // this seems to overflow a short
                lino = lino > 32767 ? 32767 : lino;
                lino = lino < -32768 ? -32768 : lino;
                return lino;//(sr << 2);    // sr was 14-bit dynamic range
                }

        default:
            return (-1);
    }
}

///////////////////////////////////////////////////////////////////////////////
// G.726 ADPCM 16kbps implementation
//

static int q_tab_16[1] = 
{
    261
    };

// Maps G.726_16 code word to reconstructed scale factor normalized log
// magnitude values.  Comes from Table 11/G.726
//
static short dqln_tab_16[4] = 
{
    116, 365, 365, 116
    }; 

// Maps G.726_16 code word to log of scale factor multiplier.
//
// wi_tab_16[4] is actually {-22 , 439, 439, -22}, but FILTD wants it
// as WI << 5  (multiplied by 32), so we'll do that here 
//
static short wi_tab_16[4] = 
{
    -704, 14048, 14048, -704
    };

// Maps G.726_16 code words to a set of values whose long and short
// term averages are computed and then compared to give an indication
// how stationary (steady state) the signal is.
//
// Comes from FUNCTF
static short fi_tab_16[4] = 
{
    0, 0xE00, 0xE00, 0
    };

int
g726_state::encoder_16
(
    int sl,
    int in_coding
    )
{
    // Linearize input sample to 16-bit PCM
    //
    switch ( in_coding )
    {    
        case AUDIO_ENCODING_ALAW:
            sl = alaw2linear( sl );
            break;
        case AUDIO_ENCODING_ULAW:
            sl = ulaw2linear( sl );
            break;
        case AUDIO_ENCODING_LINEAR:
            break;
        default:
            return -1;
        }

    // Force 14-bit sample dynamic range
    //
    sl >>= 2;

    int sezi = predictor_zero ();
    int sez = sezi >> 1;
    int sei = sezi + predictor_pole ();
    int se = sei >> 1;  // estimated signal

    int d = sl - se; // d = estimation diff.

    // quantize prediction difference d
    int y = step_size ();   // quantizer step size

    int i = quantize( d, y, q_tab_16, 1 );  // i = ADPCM code

    // Since quantize() only produces a three level output
    // (1, 2, or 3), we must create the fourth one on our own
          
    if ( i == 3 )                   // 'i' code for the zero region
        if ( ( d & 0x8000 ) == 0 )  // If d > 0, i = 3 isn't right...
            i = 0;
        
    int dq = reconstruct( i & 0x02, dqln_tab_16[i], y ); // quantized diff.

    int sr = (dq < 0) ? se - ( dq & 0x3FFF ) : se + dq; // reconstructed signal

    int dqsez = sr + sez - se;      // pole prediction diff.

    update( 2, y, wi_tab_16[i], fi_tab_16[i], dq, sr, dqsez );

    return i;
    }

int
g726_state::decoder_16
(
    int i,
    int out_coding
    )
{
    i &= 0x03;          // mask to get proper bits

    int sezi = predictor_zero ();
    int sez = sezi >> 1;
    int sei = sezi + predictor_pole ();
    int se = sei >> 1;          // se = estimated signal

    int y = step_size ();   // adaptive quantizer step size

    int dq = reconstruct( i & 0x02, dqln_tab_16[i], y ); // unquantize pred diff

    int sr = (dq < 0) ? ( se - ( dq & 0x3FFF ) ) : ( se + dq ); // reconst. signal

    int dqsez = sr + sez - se;          // pole prediction diff.

    update( 2, y, wi_tab_16[i], fi_tab_16[i], dq, sr, dqsez );

    switch ( out_coding )
    {
        case AUDIO_ENCODING_ALAW:
            return tandem_adjust_alaw( sr, se, y, i, 2, q_tab_16 );

        case AUDIO_ENCODING_ULAW:
            return tandem_adjust_ulaw( sr, se, y, i, 2, q_tab_16 );

        case AUDIO_ENCODING_LINEAR:
        {
            long lino = long( sr ) << 2; // this seems to overflow a short
            lino = lino > 32767 ? 32767 : lino;
            lino = lino < -32768 ? -32768 : lino;
            return lino; // sr was 14-bit dynamic range
            }

        default:
            return -1;
        }
    }

///////////////////////////////////////////////////////////////////////////////
// G.726 ADPCM 24kbps implementation
//

static int q_tab_24[3] = 
{
    8, 218, 331
    };

// Maps G.726_24 code word to reconstructed scale factor normalized log
// magnitude values.
//
static short dqln_tab_24[8] = 
{
    -2048, 135, 273, 373, 373, 273, 135, -2048
    };

// Maps G.726_24 code word to log of scale factor multiplier.
//
static short wi_tab_24[8] = 
{
    -128, 960, 4384, 18624, 18624, 4384, 960, -128
    };

//
// Maps G.726_24 code words to a set of values whose long and short
// term averages are computed and then compared to give an indication
// how stationary (steady state) the signal is.
//
static short fi_tab_24[8] = 
{
    0, 0x200, 0x400, 0xE00, 0xE00, 0x400, 0x200, 0
    };

int
g726_state::encoder_24
(
    int sl,
    int in_coding
    )
{
    // Linearize input sample to 16-bit PCM
    //
    switch ( in_coding )
    {    
        case AUDIO_ENCODING_ALAW:
            sl = alaw2linear( sl );
            break;
        case AUDIO_ENCODING_ULAW:
            sl = ulaw2linear( sl );
            break;
        case AUDIO_ENCODING_LINEAR:
            break;
        default:
            return -1;
        }

    // Force 14-bit sample dynamic range
    //
    sl >>= 2;

    int sezi = predictor_zero ();
    int sez = sezi >> 1;
    int sei = sezi + predictor_pole ();
    int se = sei >> 1;  // estimated signal

    int d = sl - se; // d = estimation diff.

    // quantize prediction difference d
    int y = step_size ();   // quantizer step size

    int i = quantize(d, y, q_tab_24, 3); // i = ADPCM code

    int dq = reconstruct( i & 0x04, dqln_tab_24[i], y ); // quantized diff.

    int sr = (dq < 0) ? se - (dq & 0x3FFF) : se + dq; // reconstructed signal

    int dqsez = sr + sez - se;      // pole prediction diff.

    update( 3, y, wi_tab_24[i], fi_tab_24[i], dq, sr, dqsez );

    return i;
    }

int
g726_state::decoder_24
(
    int i,
    int out_coding
    )
{
    i &= 0x07;  // mask to get proper bits

    int sezi = predictor_zero ();
    int sez = sezi >> 1;
    int sei = sezi + predictor_pole ();
    int se = sei >> 1;          // se = estimated signal

    int y = step_size ();   // adaptive quantizer step size

    int dq = reconstruct( i & 0x04, dqln_tab_24[i], y ); // unquantize pred diff

    int sr = (dq < 0) ? (se - (dq & 0x3FFF)) : (se + dq); // reconst. signal

    int dqsez = sr + sez - se;          // pole prediction diff.

    update( 3, y, wi_tab_24[i], fi_tab_24[i], dq, sr, dqsez );

    switch ( out_coding )
    {
        case AUDIO_ENCODING_ALAW:
            return tandem_adjust_alaw( sr, se, y, i, 4, q_tab_24 );

        case AUDIO_ENCODING_ULAW:
            return tandem_adjust_ulaw( sr, se, y, i, 4, q_tab_24 );

        case AUDIO_ENCODING_LINEAR:
        {
            long lino = long( sr ) << 2; // this seems to overflow a short
            lino = lino > 32767 ? 32767 : lino;
            lino = lino < -32768 ? -32768 : lino;
            return lino; // sr was 14-bit dynamic range
            }

        default:
            return -1;
        }
    }

///////////////////////////////////////////////////////////////////////////////
// G.726 ADPCM 40kbps implementation
//

static int q_tab_40[15] = 
{
    -122, -16, 68, 139, 198, 250, 298, 339,
    378, 413, 445, 475, 502, 528, 553
    };

// Maps G.726_40 code word to ructeconstructed scale factor normalized log
// magnitude values.
//
static short dqln_tab_40[32] = 
{
    -2048, -66, 28, 104, 169, 224, 274, 318,
    358, 395, 429, 459, 488, 514, 539, 566,
    566, 539, 514, 488, 459, 429, 395, 358,
    318, 274, 224, 169, 104, 28, -66, -2048
    };

// Maps G.726_40 code word to log of scale factor multiplier.
//
static short wi_tab_40[32] = 
{
    448, 448, 768, 1248, 1280, 1312, 1856, 3200,
    4512, 5728, 7008, 8960, 11456, 14080, 16928, 22272,
    22272, 16928, 14080, 11456, 8960, 7008, 5728, 4512,
    3200, 1856, 1312, 1280, 1248, 768, 448, 448
    };

// Maps G.726_40 code words to a set of values whose long and short
// term averages are computed and then compared to give an indication
// how stationary (steady state) the signal is.
//
static short fi_tab_40[32] = 
{
    0, 0, 0, 0, 0, 0x200, 0x200, 0x200,
    0x200, 0x200, 0x400, 0x600, 0x800, 0xA00, 0xC00, 0xC00,
    0xC00, 0xC00, 0xA00, 0x800, 0x600, 0x400, 0x200, 0x200,
    0x200, 0x200, 0x200, 0, 0, 0, 0, 0
    };

int
g726_state::encoder_40
(
    int sl,
    int in_coding
    )
{
    // Linearize input sample to 16-bit PCM
    //
    switch ( in_coding )
    {    
        case AUDIO_ENCODING_ALAW:
            sl = alaw2linear( sl );
            break;
        case AUDIO_ENCODING_ULAW:
            sl = ulaw2linear( sl );
            break;
        case AUDIO_ENCODING_LINEAR:
            break;
        default:
            return -1;
        }

    // Force 14-bit sample dynamic range
    //
    sl >>= 2;

    int sezi = predictor_zero ();
    int sez = sezi >> 1;
    int sei = sezi + predictor_pole ();
    int se = sei >> 1;  // estimated signal

    int d = sl - se; // d = estimation diff.

    // quantize prediction difference d
    int y = step_size ();   // quantizer step size

    int i = quantize( d, y, q_tab_40, 15 );    // i = ADPCM code

    int dq = reconstruct( i & 0x10, dqln_tab_40[i], y ); // quantized diff

    int sr = (dq < 0) ? se - (dq & 0x7FFF) : se + dq; // reconstructed signal

    int dqsez = sr + sez - se;      // dqsez = pole prediction diff.

    update( 5, y, wi_tab_40[i], fi_tab_40[i], dq, sr, dqsez );

    return i;
    }

int
g726_state::decoder_40
(
    int i,
    int out_coding
    )
{
    i &= 0x1f;              // mask to get proper bits

    int sezi = predictor_zero ();
    int sez = sezi >> 1;
    int sei = sezi + predictor_pole ();
    int se = sei >> 1;          // se = estimated signal

    int y = step_size ();   // adaptive quantizer step size

    int dq = reconstruct( i & 0x10, dqln_tab_40[i], y ); // estimation diff.

    int sr = (dq < 0) ? (se - (dq & 0x7FFF)) : (se + dq); // reconst. signal

    int dqsez = sr - se + sez;      // pole prediction diff.

    update( 5, y, wi_tab_40[i], fi_tab_40[i], dq, sr, dqsez );

    switch ( out_coding )
    {
        case AUDIO_ENCODING_ALAW:
            return tandem_adjust_alaw( sr, se, y, i, 0x10, q_tab_40 );

        case AUDIO_ENCODING_ULAW:
            return tandem_adjust_ulaw( sr, se, y, i, 0x10, q_tab_40 );

        case AUDIO_ENCODING_LINEAR:
        {
            long lino = long( sr ) << 2; // this seems to overflow a short
            lino = lino > 32767 ? 32767 : lino;
            lino = lino < -32768 ? -32768 : lino;
            return lino; // sr was 14-bit dynamic range
            }

        default:
            return -1;
        }
    }


#define SIGN_BIT    (0x80)      // Sign bit for a A-law byte. 
#define QUANT_MASK  (0xf)       // Quantization field mask. 
#define NSEGS       (8)         // Number of A-law segments. 
#define SEG_SHIFT   (4)         // Left shift for segment number. 
#define SEG_MASK    (0x70)      // Segment field mask. 

static short seg_aend[ 8 ]
    = { 0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF };

static short seg_uend[ 8 ]
    = { 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF };

// copy from CCITT G.711 specifications
//
static unsigned char _u2a[ 128 ] = // u- to A-law conversions 
{
      1,   1,   2,   2,   3,   3,   4,   4,
      5,   5,   6,   6,   7,   7,   8,   8,
      9,  10,  11,  12,  13,  14,  15,  16,
     17,  18,  19,  20,  21,  22,  23,  24,
     25,  27,  29,  31,  33,  34,  35,  36,
     37,  38,  39,  40,  41,  42,  43,  44,
     46,  48,  49,  50,  51,  52,  53,  54,
     55,  56,  57,  58,  59,  60,  61,  62,
     64,  65,  66,  67,  68,  69,  70,  71,
     72,  73,  74,  75,  76,  77,  78,  79,
     80,  82,  83,  84,  85,  86,  87,  88,
     89,  90,  91,  92,  93,  94,  95,  96,
     97,  98,  99, 100, 101, 102, 103, 104,
    105, 106, 107, 108, 109, 110, 111, 112,
    113, 114, 115, 116, 117, 118, 119, 120,
    121, 122, 123, 124, 125, 126, 127, 128
    };

static unsigned char _a2u[ 128 ] = // A- to u-law conversions 
{
      1,   3,   5,   7,   9,  11,  13,  15,
     16,  17,  18,  19,  20,  21,  22,  23,
     24,  25,  26,  27,  28,  29,  30,  31,
     32,  32,  33,  33,  34,  34,  35,  35,
     36,  37,  38,  39,  40,  41,  42,  43,
     44,  45,  46,  47,  48,  48,  49,  49,
     50,  51,  52,  53,  54,  55,  56,  57,
     58,  59,  60,  61,  62,  63,  64,  64,
     65,  66,  67,  68,  69,  70,  71,  72,
     73,  74,  75,  76,  77,  78,  79,  80,
     80,  81,  82,  83,  84,  85,  86,  87,
     88,  89,  90,  91,  92,  93,  94,  95,
     96,  97,  98,  99, 100, 101, 102, 103,
    104, 10 , 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125, 126, 127
    };

inline static short search( int val, short* table, int size )
{
    for (int i = 0; i < size; i++)
        if ( val <= *table++ )
            return i;

    return size;
    }

//
//  linear2alaw() - Convert a 16-bit linear PCM value to 8-bit A-law
// 
//  linear2alaw() accepts an 16-bit integer and encodes it as A-law data.
// 
//       Linear Input Code   Compressed Code
//   ------------------------    ---------------
//   0000000wxyza            000wxyz
//   0000001wxyza            001wxyz
//   000001wxyzab            010wxyz
//   00001wxyzabc            011wxyz
//   0001wxyzabcd            100wxyz
//   001wxyzabcde            101wxyz
//   01wxyzabcdef            110wxyz
//   1wxyzabcdefg            111wxyz
// 
//  For further information see John C. Bellamy's Digital Telephony, 1982,
//  John Wiley & Sons, pps 98-111 and 472-476.
//
unsigned char linear2alaw( int pcm_val ) // pcm_val: 2's complement (16-bit range)
{
    pcm_val = pcm_val >> 3;

    int mask;
    if ( pcm_val >= 0 )
    {
        mask = 0xD5; // sign (7th) bit = 1 
        }
    else 
    {
        mask = 0x55; // sign bit = 0 
        pcm_val = -pcm_val - 1;
        }

    // Convert the scaled magnitude to segment number. 
    //
    int seg = search( pcm_val, seg_aend, 8 );

    // Combine the sign, segment, and quantization bits. 
    //
    if ( seg >= 8 )  // out of range, return maximum value. 
        return (unsigned char)( 0x7F ^ mask );

    unsigned char aval = (unsigned char) seg << SEG_SHIFT;
    if ( seg < 2 )
        aval |= (pcm_val >> 1) & QUANT_MASK;
    else
        aval |= (pcm_val >> seg) & QUANT_MASK;
    return aval ^ mask;
    }

//
// alaw2linear() - Convert an A-law value to 16-bit linear PCM
//
int alaw2linear( unsigned char a_val )
{
    a_val ^= 0x55;

    int t = (a_val & QUANT_MASK) << 4;
    int seg = ((unsigned)a_val & SEG_MASK) >> SEG_SHIFT;
    switch ( seg )
    {
        case 0:
            t += 8;
            break;
        case 1:
            t += 0x108;
            break;
        default:
            t += 0x108;
            t <<= seg - 1;
        }

    return ( a_val & SIGN_BIT ) ? t : -t;
    }

#define BIAS        (0x84)      // Bias for linear code. 
#define CLIP        8159

//
// linear2ulaw() - Convert a linear PCM value to u-law
//
// In order to simplify the encoding process, the original linear magnitude
// is biased by adding 33 which shifts the encoding range from (0 - 8158) to
// (33 - 8191). The result can be seen in the following encoding table:
//
//  Biased Linear Input Code    Compressed Code
//  ------------------------    ---------------
//  00000001wxyza           000wxyz
//  0000001wxyzab           001wxyz
//  000001wxyzabc           010wxyz
//  00001wxyzabcd           011wxyz
//  0001wxyzabcde           100wxyz
//  001wxyzabcdef           101wxyz
//  01wxyzabcdefg           110wxyz
//  1wxyzabcdefgh           111wxyz
//
// Each biased linear code has a leading 1 which identifies the segment
// number. The value of the segment number is equal to 7 minus the number
// of leading 0's. The quantization interval is directly available as the
// four bits wxyz. // The trailing bits (a - h) are ignored.
//
// Ordinarily the complement of the resulting code word is used for
// transmission, and so the code word is complemented before it is returned.
//
// For further information see John C. Bellamy's Digital Telephony, 1982,
// John Wiley & Sons, pps 98-111 and 472-476.
//
unsigned char linear2ulaw( int pcm_val ) // pcm_val: 2's complement (16-bit range) 
{
    // Get the sign and the magnitude of the value. 
    pcm_val = pcm_val >> 2;
    short mask;
    if (pcm_val < 0)
    {
        pcm_val = -pcm_val;
        mask = 0x7F;
        } 
    else
    {
        mask = 0xFF;
        }

    if ( pcm_val > CLIP )
        pcm_val = CLIP; // clip the magnitude 

    pcm_val += (BIAS >> 2);

    // Convert the scaled magnitude to segment number. 
    //
    short seg = search( pcm_val, seg_uend, 8 );

    // Combine the sign, segment, quantization bits;
    // and complement the code word.
    //
    if ( seg >= 8 ) // out of range, return maximum value. 
        return (unsigned char)( 0x7F ^ mask );

    unsigned char uval = (unsigned char)(seg << 4) 
        | ( ( pcm_val >> ( seg + 1 ) ) & 0xF );

    return ( uval ^ mask );
    }

//
// ulaw2linear() - Convert a u-law value to 16-bit linear PCM
//
// First, a biased linear code is derived from the code word. An unbiased
// output can then be obtained by subtracting 33 from the biased code.
//
// Note that this function expects to be passed the complement of the
// original code word. This is in keeping with ISDN conventions.
// 
int ulaw2linear( unsigned char u_val )
{
    // Complement to obtain normal u-law value. 
    u_val = ~u_val;

    // Extract and bias the quantization bits. Then
    // shift up by the segment number and subtract out the bias.
    //
    short t = ( ( u_val & QUANT_MASK ) << 3 ) + BIAS;
    t <<= ( (unsigned)u_val & SEG_MASK ) >> SEG_SHIFT;

    return ( ( u_val & SIGN_BIT ) ? ( BIAS - t ) : ( t - BIAS ) );
    }

// A-law to u-law conversion 
//
unsigned char alaw2ulaw( unsigned char aval )
{
    aval &= 0xff;
    return (unsigned char)( ( aval & 0x80 ) 
        ? ( 0xFF ^ _a2u[ aval ^ 0xD5 ] )
        : ( 0x7F ^ _a2u[ aval ^ 0x55 ] ) );
    }

// u-law to A-law conversion 
//
unsigned char ulaw2alaw( unsigned char uval )
{
    uval &= 0xff;
    return (unsigned char)( 
        (uval & 0x80) 
        ? ( 0xD5 ^ ( _u2a[ 0xFF ^ uval ] - 1 ) ) 
        : (unsigned char)( 0x55 ^ ( _u2a[ 0x7F ^ uval ] - 1 ) )
        );
    }
