/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2005 D2 TECHNOLOGIES, INC ALL RIGHTS RESERVED"
 *
 * THIS IS AN UNPUBLISHED WORK CONTAINING VIRATA CORPORATION CONFIDENTIAL AND
 * PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2002 VIRATA CORPORATION ALL RIGHTS RESERVED"
 *
 * ======== +RCSfile: comm.h + ========
 *
 * "@(#) +ProjectRevision: 54.3.7.1 +"
 *
 */

#ifndef _COMM
#define _COMM

#ifndef NULL
#define NULL (0)
#endif

#define COMM_STACKMEM (5) /* Max. Scratch usage for the COMM module */

#ifndef _GLOBAL_PARAMS
#define _GLOBAL_PARAMS

typedef struct {
    unsigned p0DBIN;  /* Reference input level for 0dBm */
    unsigned p0DBOUT; /* Reference output level for 0dBm */
} GLOBAL_Params;
#endif

#define COMM_LEN            (40)
#define COMM_LEN2           (COMM_LEN/2)
#define COMM_LEN4           (COMM_LEN/4)
#define COMM_RATE           (8000) /* Sampling Rate */
#define COMM_MAX_BLOCK_SIZE (80) /* 10ms block */
#define COMM_MIN_BLOCK_SIZE (40) /* 5ms block */

/*  Prototypes for Common Functions shared across Algorithms */
void mu2linearMSB(
    int   blockSize,     /* size of array to convert (in samples) */
    void *muBuf_ptr,     /* pointer to input mu-law array (MSB first) */
    void *buf_ptr);      /* pointer to output Linear array */
void linear2muMSB(
    int   blockSize,     /* size of array to convert (in samples) */
    void *buf_ptr,       /* pointer to input Linear array */
    void *muBuf_ptr);    /* pointer to output mu-law array (MSB first) */
void a2linearMSB(
    int   blockSize,     /* size of array to convert (in samples) */
    void *aBuf_ptr,      /* pointer to input a-law array (MSB first) */
    void *buf_ptr);      /* pointer to output Linear array */
void linear2aMSB(
    int   blockSize,     /* size of array to convert (in samples) */
    void *aBuf_ptr,      /* pointer to input Linear array */
    void *buf_ptr);      /* pointer to output a-law array (MSB first) */
void mu2linearLSB(
    int   blockSize,     /* size of array to convert (in samples) */
    void *muBuf_ptr,     /* pointer to input mu-law array (LSB first) */
    void *buf_ptr);      /* pointer to output Linear array */
void linear2muLSB(
    int   blockSize,     /* size of array to convert (in samples) */
    void *buf_ptr,       /* pointer to input Linear array */
    void *muBuf_ptr);    /* pointer to output mu-law array (LSB first) */
void a2linearLSB(
    int   blockSize,     /* size of array to convert (in samples) */
    void *aBuf_ptr,      /* pointer to input a-law array (LSB first) */
    void *buf_ptr);      /* pointer to output Linear array */
void linear2aLSB(
    int   blockSize,     /* size of array to convert (in samples) */
    void *aBuf_ptr,      /* pointer to input Linear array */
    void *buf_ptr);      /* pointer to output a-law array (LSB first) */
void COMM_mu2linear(
    int   blockSize,     /* size of array to convert (in samples) */
    void *muBuf_ptr,     /* pointer to input mu-law array (bit 0 to bit 7) */
    void *buf_ptr);      /* pointer to output Linear array */
void COMM_linear2mu(
    int   blockSize,     /* size of array to convert (in samples) */
    void *buf_ptr,       /* pointer to input Linear array */
    void *muBuf_ptr);    /* pointer to output mu-law array (bit 0 to bit 7) */
void COMM_a2linear(
    int   blockSize,     /* size of array to convert (in samples) */
    void *aBuf_ptr,      /* pointer to input a-Law array (bit 0 to bit 7) */
    void *buf_ptr);      /* pointer to output Linear array */
void COMM_linear2a(
    int   blockSize,     /* size of array to convert (in samples) */
    void *buf_ptr,       /* pointer to input Linear array */
    void *aBuf_ptr);     /* pointer to output a-Law array (bit 0 to bit 7) */

void timeron();          /* timer on function */
void timeron10();        /* timer on function */
unsigned int timeroff(); /* timer off function */

void zero(
    int   len,                  /* length of array to be zeroed */
    void *z_ptr);               /* pointer to the array to be zeroed */
int  lrms(
    int   blockSize,            /* size of array to process (in samples) */
    void *srcBuf);              /* pointer to source array */
void copy(
    int   len,                  /* length of data to be copied */
    void *src_ptr,              /* pointer to source array */
    void *dst_ptr);             /* pointer to destination array */
void scale(
    int       dB2,              /* twice the req. dB level to be scaled */
    unsigned  intNum,           /* number of samples to scale */
    void     *src_ptr,          /* pointer to the source buffer */
    void     *dst_ptr);         /* pointer to the destination buffer */
int reldB(
    int    rmsVal,              /* RMS value */
    int    rmsRelative);        /* reference level */
void attn(
    int       dB2,              /* twice the req. dB level to be attenuated */
    unsigned  intNum,           /* number of samples to attenuate */
    void     *src_ptr,          /* pointer to the source buffer */
    void     *dst_ptr);         /* pointer to the destination buffer */
void shift(
    int  blkSize,               /* block size for src and dst in samples */
    int *src_ptr,               /* source buffer pointer */
    int *dst_ptr,               /* destination buffer pointer */
    int  shiftValue);           /* shift value, -16 to 15 */
void sum(
    int  blkSize,               /* block size for src and dst in samples */
    int *src1_ptr,              /* source buffer 1 pointer */
    int *src2_ptr,              /* source buffer 2 pointer */
    int *dst_ptr);              /* destination buffer pointer */

#endif
