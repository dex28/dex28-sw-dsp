/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2005 D2 TECHNOLOGIES, INC ALL RIGHTS RESERVED"
 *
 * THIS IS AN UNPUBLISHED WORK CONTAINING VIRATA CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2001 VIRATA ALL RIGHTS RESERVED"
 *
 *
 * ======== +RCSfile: ecsr.h + ========
 *
 * "@(#) +ProjectRevision: 54.3.10.9 +"
 *
 *  Single reflector echo canceller with configurable tail lengths.
 *
 *  ******** Object file descriptions: ********
 *  ecsr.o54: contains ECSR_init() and ECSR_run(), normal C mode.
 *  ecsr.f54: contains ECSR_init() and ECSR_run(), far C mode.
 *  ecsr_isr.o54: contains ECSR_isrRun(), ECSR_run(), normal C mode.
 *  ecsr_isr.f54: contains ECSR_isrRun(), ECSR_run(), far C mode.
 *    Program(.text):          
 *       ECSR_init, ECSR_run:               1889 words
 *       ECSR_init, ECSR_run, ECSR_isrRun:                       1994 words
 *    Table(.data):                           19 words             19 word
 *    Total:                                1908 words           2006 word
 *
 *  ******** MIPS/Memory Performance ********
 *  MIPS measurement is based on internal memory object
 *
 *  ECSR_init():
 *      Tail length:                            16ms    32ms    48ms                
 *      # cycles:                                696     952    1208
 *      Object memory / port:                    520     776    1032
 *      C stack usage:                             0
 *      VP Open stack usage:                    none
 *      VP Open B1 memory usage:                none
 *
 *  ECSR_run(): (without ISR)
 *      Tail length:                            16ms    32ms   48ms                  
 *      MIPS:                                    2.4     2.9    3.5
 *      Object memory / port:                    520     776    1032
 *      C stack usage:                             0
 *      VP Open stack usage:                    ECSR_xxMS_STACKMEMSIZE
 *      VP Open B1 memory usage:                 none
 *
 *  ECSR_run() + ECSR_isrRun: 
 *      Tail length:                            16ms    32ms   48ms                   
 *      MIPS (ISR block size = 1):               3.6     4.1    4.6
 *      MIPS (ISR block size = 2):               3.0     3.5    4.0
 *      MIPS (ISR block size = 4):               2.8     3.2    3.7
 *      MIPS (ISR block size = 8):               2.6     3.0    3.5
 *      Object memory / port:                   ECSR_run object memory / port 
 *                                              + (6 * block size)   
 *      C stack usage:                            0
 *      VP Open stack usage for ECSR_run:       ECSR_xxMS_STACKMEMSIZE
 *      VP Open stack usage for ECSR_isrRun:    none
 *      VP Open B1 memory usage:                none
 *
 *       definition of control and status words in a ec_obj:
 *       CONTROL: 15 14 13  12  11  10   9   8   7  6  5  4  3  2  1  0
 *                          ISR ERL     FRZ BYP NLP  
 *       control word -
 *         ISR: disable/enable ISR (1/0)
 *         ERL: select worst ERL (1 -> 6 dB, 0 -> 0 dB)
 *         FRZ: enable/disable (1/0) freeze of coefficients
 *         BYP: enable/disable (1/0) bypass of the input signal
 *         NLP: enable/dissable nonlinear processing (1/0)
 *
 *       STATUS:  15 14 13  12  11  10   9   8    7       6    5  4  3  2  1  0
 *                         ICV DTK LCK FRZ CVG NLP_BLK NLP_ISR 
 *       status word -
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

#ifndef _ECSR
#define _ECSR

/* initialization mode */
#define ECSR_COLDSTART (0)
#define ECSR_WARMSTART (1)

/* control word masks */
#define ECSR_NLP            (128)   /* 0x0080, NLP enable control bit */
#define ECSR_BYP            (256)   /* 0x100 */
#define ECSR_FRZ            (512)   /* 0x200, also use for status word */
#define ECSR_ERL            (2048)  /* 0x800 */
#define ECSR_ISR_DISABLE    (8192)  /* 0x2000 */

/* status word masks */
#define ECSR_NLP_ISR_ACT    (64)    /* 0x0040 */
#define ECSR_NLP_BLK_ACT    (128)   /* 0x0080 */
#define ECSR_CVG            (256)   /* 0x100  */
#define ECSR_LCK            (1024)  /* 0x0400 */
#define ECSR_DTK            (2048)  /* 0x0800 */
#define ECSR_ICV            (4096)  /* 0x1000 */

/* default parameters */

#define ECSR_BLK_MIN    (40)
#define ECSR_BLK_MAX    (80)
#define ECSR_BLK_DEF    (40)

#define ECSR_MIN_DLY    (128)
#define ECSR_MAX_DLY    (384)

#define ECSR_MIN_VOICE_PWR_MIN  (-78)
#define ECSR_MIN_VOICE_PWR_MAX  (-40)
#define ECSR_MIN_VOICE_PWR_DEF  (0)

#define ECSR_VOICE_HNG_CNT_MIN  (25)
#define ECSR_VOICE_HNG_CNT_MAX  (1000)
#define ECSR_VOICE_HNG_CNT_DEF  (100)


/* delay buffer size without running ISR function */

#define ECSR_16MS_DLY_BUF_SIZE  (320)
#define ECSR_32MS_DLY_BUF_SIZE  (576)
#define ECSR_48MS_DLY_BUF_SIZE  (832)

/* 
 * when running ISR function, 
 * delay buffer size =  ECSR_xxMS_DLY_BUF_SIZE + (6 * block size)
 */

#define ECSR_16MS_STACKMEMSIZE (493)
#define ECSR_32MS_STACKMEMSIZE (740)
#define ECSR_48MS_STACKMEMSIZE (996)

/* local parameter definition */
typedef struct {
    int pBLOCKSIZE;     /* block size for ECSR_run() */
    int pMAXDELAY;      /* tail length */
    int pISRBLKSIZE;    /* ISR block size for ECSR_isrRun() */
    int pMINVOICEPOWER; /* min power level for voice active */
    int tVOICEHANGOVER; /* NLP hangover time from inactive to active */
} ECSR_Params;

typedef struct {
    int    internal[192];
} _ECSR_Internal;

typedef struct {
    _ECSR_Internal  internal;   
    unsigned        control;
    unsigned        status;
    int            *rin_ptr;       /* pointer to far end input buffer[block_size] */
    int            *sin_ptr;       /* pointer to near end input buffer[block_size] */
    int            *nlpOut_ptr;    /* (i) pointer to NLP output buffer */
    int            *soutFull_ptr;  /* (i) full block sout buffer pointer */
    int            *dlyBuf_ptr;    /* pointer to buffer[2*delay_size + EC_NTAPS] */
    int            *rinFull_ptr;   /* full block rin buffer pointer */
    int            *rinDly_ptr;    /* rin delay buffer pointer */
} ECSR_Obj;

typedef ECSR_Obj *ECSR_Handle;

void ECSR_init(
    ECSR_Handle    ecsr_ptr,        /* pointer to local ECSR object */
    GLOBAL_Params *global_ptr,      /* pointer to global parameter */
    ECSR_Params   *ecsr_params_ptr, /* pointer to local ECSR parameter */
    int            mode,            /* initialization mode (COLD/WARM) */
    int           *stack_ptr);      /* pointer to stack memory */
void ECSR_run(
    ECSR_Handle  ecsr_ptr,          /* pointer to local ECSR object */
    NFE_Handle   nfe_ptr);          /* pointer to local NFE object */
void ECSR_isrRun(
    ECSR_Handle  ecsr_ptr);         /* pointer to local ECSR object */
#endif

