/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2005 D2 TECHNOLOGIES, INC ALL RIGHTS RESERVED"
 *
 * THIS IS AN UNPUBLISHED WORK CONTAINING VIRATA CORPORATION CONFIDENTIAL AND
 * PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2000 VIRATA CORPORATION ALL RIGHTS RESERVED"
 *
 * ======== +RCSfile: g726.h + ========
 *
 * "@(#) +ProjectRevision: 54.3.1.4 +"
 *
 */
/*
 *  ITU G.726 ADPCM vocoder.
 *
 * LINEAR: (ENC_LINEAR=1, DEC_LINEAR=1)
 *  Performance:
 *        INIT    .text       .data     total   object size      MIPS
 *   Encoder:
 *         59      841         222       1122            58       4.1
 *   Decoder:
 *         59      819         222       1100            58       3.8
 *
 *   Full duplex:
 *        INIT    .text       .data     total   object size      MIPS
 *         59*    1510**       226***    1795           116       7.9
 *
 * (*): 59 words are shared by encoder and decoder initialization functions.
 * (**): 150 words are shared by encoder and decoder run functions.
 * (***): encoder and decoder share the same 218 words of data.
 * 
 * PCM: (ENC_LINEAR=1, DEC_LINEAR=0)
 *  Performance:
 *        INIT    .text       .data     total   object size      MIPS
 *   Encoder:
 *         59      841         222       1122            58       4.1
 *   Decoder:
 *         62      985        1758       2801            58       4.6
 *
 *   Full duplex:
 *        INIT    .text       .data     total   object size      MIPS
 *        121     1676*       1762**     3559           116       8.7
 *
 *  (*): 150 words are shared by encoder and decoder run functions.
 * (**): encoder and decoder share the same 218 words of data.
 * 
 *  Notes:
 *     The MIPS is based on placing .text and .data sections into DARAM
 *
 *     G726_encode() returns the number of packed words in the output buffer.
 *     The enc_obj.src_ptr points to a data buffer of size 'frameLen'.
 * 
 *     G726_decode() returns the number of packed words in the input 
 *     parameter buffer.
 *     The dec_obj.src_ptr points to a parameter buffer of packed words.
 *
 */

#ifndef _G726
#define _G726

#define G726_ENCODE (0)
#define G726_DECODE (1)

/*
 * The stack memory size used by G726 is dependent on the number of samples
 * processed per call (frameLen calling argument to the G726_init() function).
 * Stack memory used is the sum of frameLen + G726_E_0MS_STACKMEMSIZE for the 
 * encoder and frameLen + G726_D_0MS_STACKMEMSIZEF for the decoder.
 * The following constants define the sizes for the stack space and any
 * required aligned memory sections for the Encoder and Decoder.
 */
#define G726_E_0MS_STACKMEMSIZE   (142)	
#define G726_D_0MS_STACKMEMSIZE   (142)	
#define G726_E_5MS_STACKMEMSIZE   (182)	/* G726_E_0MS_STACKMEMSIZE + frameLen */
#define G726_D_5MS_STACKMEMSIZE   (182)	/* G726_D_0MS_STACKMEMSIZE + frameLen */
#define G726_E_10MS_STACKMEMSIZE  (222)	/* G726_E_0MS_STACKMEMSIZE + frameLen */
#define G726_D_10MS_STACKMEMSIZE  (222)	/* G726_D_0MS_STACKMEMSIZE + frameLen */

typedef struct G726_IntObj {
    int    internal[56];
} _G726_Internal;	   /* same for Coder and Decoder */

/*
 *  ======== G726 OBJECT ========
 *
 * This is the data structure for the G.726 object.
 * The Encoder and Decoder both use an object with the same definition,
 * but each routine requires a separately allocated object.
 * total of 58 words for G726 object
 */
typedef struct {
   _G726_Internal  internal;
   int            *src_ptr;
   int            *dst_ptr;
} G726_Obj;

typedef G726_Obj     G726_EObj;
typedef G726_Obj     G726_DObj;
typedef G726_Obj    *G726_Handle;
typedef G726_Handle  G726_EHandle;
typedef G726_Handle  G726_DHandle;

/* 
 * Function prototypes
 */
void G726_init(
    void *obj_ptr,
    int   mode,		 /* G726_ENCODE or G726_DECODE (currently ignored) */
    int  *stack_ptr, /* if non-zero, replace Stack Pointer with this */
    int   frameLen,	 /* number of samples per frame */
    int   rate,    	 /* bits/sample {2,3,4,5} */
    int   law);		 /* 0=ALaw, 1=MuLaw (ignored in LINEAR mode) */

int G726_encode(
    G726_EHandle enc_ptr);

int G726_decode(
    G726_DHandle dec_ptr);

#endif /* ifdef _G726 */
