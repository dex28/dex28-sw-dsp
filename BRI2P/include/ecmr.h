/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2005 D2 TECHNOLOGIES, INC ALL RIGHTS RESERVED"
 *
 * THIS IS AN UNPUBLISHED WORK CONTAINING VIRATA CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2001 VIRATA ALL RIGHTS RESERVED"
 *
 * ======== +RCSfile: ecmr.h + ========
 *
 * "@(#) +ProjectRevision: 54.3.10.9 +"
 *
 *  Multiple reflector echo canceller with configurable tail lengths.
 *
 *  ******** Object file descriptions: ********
 *  ecmr.o54: contains ECMR_init() and ECMR_run(), normal C mode.
 *  ecmr.f54: contains ECMR_init() and ECMR_run(), far C mode.
 *    Program(.text):                       (without ISR)          (with ISR)
 *       ECMR_init, ECMR_run:                 2829 words
 *       ECMR_init, ECMR_run, ECMR_isrRun:                          3028 words
 *    Table(.data):                             35 words              35 words
 *    Total:                                  2964 words            3063 words
 *
 *  ******** MIPs/Memory Performance ********
 *
 *  ECMR_init():
 *      Tail length:                32ms  48ms  64ms  80ms  96ms  112ms  128ms                      
 *      # cycles:                   3514  4538  5562  6586  7610  8634   9658
 *      Object memory /port:        1025  1281  1537  1793  2049  2305   2561
 *      C stack usage:              0
 *      VP Open stack usage:        none
 *      VP Open B1 memory usage:    none
 *
 *  ECMR_run(): (tested with 0 wait state external data memory)
 *      (without ISR, MIPs are based on external program and object memory)
 *      Tail length:                32ms  48ms  64ms  80ms  96ms  112ms  128ms                
 *      MIPs:                       5.5   6.1   6.8   7.5   8.2   8.9    9.5 
 *      Object memory /port:        1025  1281  1537  1793  2049  2305   2561
 *      C stack usage:              0
 *      VP Open stack usage:        ECMR_xxMS_STACKMEMSIZE
 *      VP Open B1 memory usage:    none
 *
 *  ECMR_run() + ECMR_isrRun(): (tested with internal data memory)
 *      (MIPs are based on external program memory and internal object memory)
 *      Tail length:                32ms  48ms  64ms  80ms  96ms  112ms  128ms          
 *      MIPs (ISR block size = 1):  5.6   6.0   6.5   6.9   7.4   7.8    8.3
 *      MIPs (ISR block size = 2):  5.1   5.5   6.0   6.4   6.9   7.3    7.8
 *      MIPs (ISR block size = 4):  4.8   5.3   5.7   6.1   6.6   7.0    7.5
 *      MIPs (ISR block size = 8):  4.7   5.2   5.6   6.0   6.5   6.9    7.4
 *      Object memory / port:    (ECMR_run object memory / port) + (6 * block size)   
 *      C stack usage:           0
 *      VP Open stack usage:     ECMR_xxMS_STACKMEMSIZE
 *      VP Open B1 memory usage: none
 *
 *       definition of control and status words in a ec_obj:
 *       CONTROL: 15 14 13  12  11  10   9   8   7  6  5  4  3  2  1  0
 *                     ISR    ERL2 ERL1 FRZ BYP NLP  
 *       control word -
 *         ISR: ECMR_isrRun disable bit
 *         ERL2 ERL1: select worst ERL (00 -> 0 dB, 01 -> 3 dB, 10 -> 6 dB)
 *         FRZ: enable/disable (1/0) freeze of coefficients
 *         BYP: enable/disable (1/0) bypass of the input signal
 *         NLP: enable/dissable nonlinear processing (1/0)
 *
 *       STATUS:  15 14 13  12  11  10   9   8    7       6    5  4  3  2  1  0
 *                  ILK    ICV DTK LCK FRZ CVG NLP_BLK NLP_ISR  
 *       status word -
 *         ILK: initial delay locking status
 *         ICV: initial convergence
 *         DTK: whether the double talker condition is ture or not (1/0)
 *         LCK: whether the echo delay has been locked or not (1/0)
 *         FRZ: whether the coefficients are freezed or not (1/0)
 *         CVG: whether the convergence is reached or not (1/0)
 *         NLP_BLK: nonlinear processing active flag for the main block (1/0)
 *         NLP_ISR: nonlinear processing active flag for the ISR (1/0)
 *
 */


#include <comm.h>
#include <nfe.h>

#ifndef _ECMR
#define _ECMR

/* initialization mode */
#define ECMR_COLDSTART (0)
#define ECMR_WARMSTART (1)

#define ECMR_NLP            (128)   /* 0x0080, NLP enable control bit */
#define ECMR_BYP            (256)   /* 0x0100 */
#define ECMR_FRZ            (512)   /* 0x0200, also use for status word */
#define ECMR_ERL1           (1024)  /* 0x0400 */
#define ECMR_ERL2           (2048)  /* 0x0800 */
#define ECMR_BULK           (4096)  /* 0X1000 */
#define ECMR_ISR_DISABLE    (8192)  /* 0x2000 */

/* status word masks */
#define ECMR_NLP_ISR_ACT    (64)    /* 0x0040 */
#define ECMR_NLP_BLK_ACT    (128)   /* 0x0080 */
#define ECMR_CVG            (256)   /* 0x0100 */
#define ECMR_LCK            (1024)  /* 0x0400 */
#define ECMR_DTK            (2048)  /* 0x0800 */
#define ECMR_ICV            (4096)  /* 0x1000 */
#define ECMR_ILK            (16384) /* 0x4000 */


#define ECMR_BLK_MIN    (40)    /* minimum block size,  5 ms */
#define ECMR_BLK_MAX    (80)    /* maximum block size, 10 ms */
#define ECMR_BLK_DEF    (40)    /* default block size,  5 ms */

#define ECMR_MIN_VOICE_PWR_MIN  (-78)
#define ECMR_MIN_VOICE_PWR_MAX  (-40)
#define ECMR_MIN_VOICE_PWR_DEF  (0)

#define ECMR_VOICE_HNG_CNT_MIN  (25)
#define ECMR_VOICE_HNG_CNT_MAX  (1000)
#define ECMR_VOICE_HNG_CNT_DEF  (100)


/* delay buffer size */
#define ECMR_32MS_DLY_BUF_SIZE  (576)     
#define ECMR_48MS_DLY_BUF_SIZE  (832)     
#define ECMR_64MS_DLY_BUF_SIZE  (1088)    
#define ECMR_80MS_DLY_BUF_SIZE  (1344)    
#define ECMR_96MS_DLY_BUF_SIZE  (1600)    
#define ECMR_112MS_DLY_BUF_SIZE (1856)    
#define ECMR_128MS_DLY_BUF_SIZE (2112)        

/* tail length */
#define ECMR_MIN_DLY          (256)     /*  32 ms tail length */
#define ECMR_MAX_DLY          (1024)    /* 128 ms tail length */

/* echo paths*/
#define ECMR_MAX_ECHO_PATHS   (3)       /* max number of echo paths */

#define ECMR_32MS_STACKMEMSIZE (744)
#define ECMR_48MS_STACKMEMSIZE (1000)
#define ECMR_64MS_STACKMEMSIZE (1256)
#define ECMR_80MS_STACKMEMSIZE (1512)
#define ECMR_96MS_STACKMEMSIZE (1768)
#define ECMR_112MS_STACKMEMSIZE (2024)
#define ECMR_128MS_STACKMEMSIZE (2280)

/* local parameter definition */
typedef struct {
    int pBLOCKSIZE;     /* block size for ECMR_run() */
    int pMAXDELAY;      /* tail length */
    int pISRBLKSIZE;    /* ISR block size for ECMR_isrRun() */
    int pMINVOICEPOWER; /* min power level for voice active */
    int tVOICEHANGOVER; /* NLP hangover time from inactive to active */
} ECMR_Params;


typedef struct {
    int    internal[435];
} _ECMR_Internal;

typedef struct {
    _ECMR_Internal  internal;   
    unsigned        control;        /* (i) */
    unsigned        status;         /* (o) */
    int            *rin_ptr;        /* (i) pointer to far end input buffer */
    int            *sin_ptr;        /* (i) pointer to near end input buffer */
    int            *nlpOut_ptr;     /* (i) pointer to NLP output buffer */
    int            *soutFull_ptr;   /* (i) full block sout buffer pointer */
    int            *dlyBuf_ptr;     /* (i) pointer to buffer[2*delay_size + EC_NTAPS] */
    int            *rinFull_ptr;    /* (o) full block rin buffer pointer */
    int            *rinDly_ptr;     /* (o) rin delay buffer pointer */
    int             echoPaths;      /* (o) number of echo paths */
    int             dlyStatus[3];   /* (o) echo delay and impulse response length in ms */ 
} ECMR_Obj;

typedef ECMR_Obj *ECMR_Handle;

void ECMR_init(
    ECMR_Handle    ecmr_ptr,        /* pointer to local ECMR object */
    GLOBAL_Params *global_ptr,      /* pointer to global parameter */
    ECMR_Params   *ecmrParams_ptr,  /* pointer to local ECMR parameter */
    int            mode,            /* initialization mode (COLD/WARM) */
    int           *stack_ptr);      /* pointer to stack memory */
void ECMR_run(
    ECMR_Handle  ecmr_ptr,          /* pointer to local ECMR object */
    NFE_Handle   nfe_ptr);          /* pointer to local NFE object */
void ECMR_isrRun(
    ECMR_Handle  ecmr_ptr);         /* pointer to local ECMR object */

#endif

