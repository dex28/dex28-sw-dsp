/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC. CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES:
 * "COPYRIGHT 2003-5 D2 TECHNOLOGIES, INC. ALL RIGHTS RESERVED"
 *
 * THIS IS AN UNPUBLISHED WORK CONTAINING VIRATA CORPORATION CONFIDENTIAL AND
 * PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2000 VIRATA CORPORATION ALL RIGHTS RESERVED"
 *
 * ======== +RCSfile: fmtd.h + ========
 *
 * "@(#) +ProjectRevision: 54.3.6.3 +"
 *
 *
 *   ******** Object file descriptions: ********
 *   fmtd.o54: contains FMTD_init() and FMTD_detect(), FMTD_fullDuplexDetect
 *             NEAR calls.
 *   fmtd.f54: contains FMTD_init() and FMTD_detect(), FMTD_fullDuplexDetect
 *             FAR calls.
 *
 *     Program(.text):   1286
 *     Table(.data):       13
 *     Total:            1299
 * 
 *   ******** MIPs/Memory Performance ********
 * 
 *   FMTD_init():
 *       # cycles                : 798
 *       Object memory /port     : 117
 *       C stack usage           : 0
 *       VP Open stack usage     : none
 *       VP Open B1 memory usage : none
 *   FMTD_detect():
 *       MIPs:  5 ms blockSize   : 0.290 MIPS
 *             10 ms blockSize   : 0.203 MIPS
 *       Object memory /port     : 117
 *       C stack usage           : 3
 *       VP Open stack usage     : FMTD_STACKMEMSIZE
 *       VP Open B1 memory usage : none
 *
 *   FMTD_fullDuplexDetect(): (need to initialize each object separately)
 *       MIPs:  5 ms blockSize   : 0.5798 MIPS
 *             10 ms blockSize   : 0.4051 MIPS
 *       Object memory /port     : 234
 *       C stack usage           : 7
 *       VP Open stack usage     : FMTD_STACKMEMSIZE
 *       VP Open B1 memory usage : none 
 *   Note: When configured to run using the existing stack for VP Open stack
 *         usage, FMTD will use the sum of (C stack usage plus VP Open stack
 *         usage) of stack space.
 *   Note: FMTD_init(), and FMTD_detect() share the port object.
 *
 *   MIPS performance:
 *
 *   Blocksize                    MIPS (worst case)   
 * FMTD_detect  
 *    5 ms (40 samples)           0.290 
 *   10 ms (80 samples)           0.203 
 *  
 * FMTD_fullDuplexDetect  
 *    5 ms (40 samples)           0.5798
 *   10 ms (80 samples)           0.4051 
 */

#ifndef _FMTD_H
#define _FMTD_H

/* control and status flags */
#define FMTD_FC_BIT          (0)
#define FMTD_MC_BIT          (1)
#define FMTD_ANSWER_BIT      (2)
#define FMTD_PHASE_BIT       (3)
#define FMTD_V21_BIT         (4)
#define FMTD_BA1_BIT         (5)
#define FMTD_BA2_BIT         (6)

/* control and status words */
#define FMTD_SIL             (0)
#define FMTD_FC_WORD         (1 << FMTD_FC_BIT)
#define FMTD_MC_WORD         (1 << FMTD_MC_BIT)       
#define FMTD_ANSWER_WORD     (1 << FMTD_ANSWER_BIT)     
#define FMTD_PHASE_WORD      (1 << FMTD_PHASE_BIT)      
#define FMTD_V21_WORD        (1 << FMTD_V21_BIT)
#define FMTD_BA1_WORD        (1 << FMTD_BA1_BIT)
#define FMTD_BA2_WORD        (1 << FMTD_BA2_BIT)

/* recommended default parameters */
#define FMTD_MINMAK_1100     (420)   /* min/max make/break for FAX tone */
#define FMTD_MAXMAK_1100     (580)   /* time in msec */
#define FMTD_MINBRK_1100     (2545)
#define FMTD_MAXBRK_1100     (3800)  /* Allow for USR 14.4 FAX/MODEM */
#define FMTD_MINMAK_1300     (490)   /* min/max make for Modem calling tone */
#define FMTD_MAXMAK_1300     (710)   /* time in msec */
#define FMTD_MINBRK_1300     (1490)  /* min/max make for Modem calling tone */
#define FMTD_MAXBRK_1300     (2010)  /* time in msec */ 
#define FMTD_MINMAK_2025     (1000)  /* min duration for 2025 Hz tone */
#define FMTD_MINMAK_2225     (1000)  /* min duration for 2025 Hz tone */
#define FMTD_MINMAK_PHASE    (410)   /* min/max interval for phase change */
#define FMTD_MAXMAK_PHASE    (490)   /* time in msec */

#define FMTD_BLK_SIZE        (40)    /* Default user block size */
#define FMTD_MINBLK_SIZE     (40)    /* minimum block size */
#define FMTD_MAXBLK_SIZE     (80)    /* maximum block size */

/*
 *  local variables structure
 */
typedef struct {
    int  tMINMAKE_1100;              /* FAX calling tone parameters */
    int  tMAXMAKE_1100;
    int  tMINBREAK_1100;
    int  tMAXBREAK_1100;
    int  tMINMAKE_1300;              /* MODEM calling tone parameters */
    int  tMAXMAKE_1300;
    int  tMINBREAK_1300;
    int  tMAXBREAK_1300;
    int  tMINMAKE_2025;              /* minimum duration for 2025 Hz tone */
    int  tMINMAKE_2225;              /* minimum duration for 2225 Hz tone */
    int  tMINMAKE_PH;                /* phase change parameters */
    int  tMAXMAKE_PH;
	int  pBLOCKSIZE;                 /* number of samples per user data block */
} FMTD_Params;


#define FMTD_STACKMEMSIZE (183)

typedef struct {
    int    internal[113];
} _FMTD_internal; 

typedef struct {
    _FMTD_internal  internal;
    int	           early;      
    int	           detected;   
    int            control;    
    int           *src_ptr;    
} FMTD_Obj;

typedef FMTD_Obj *FMTD_Handle;

/* function prototype */

void FMTD_init(                      /* For FMTD object initialization */
        FMTD_Handle    fmtd_ptr,     /* pointer to struct of FMTD object */
		GLOBAL_Params *global_ptr,   /* ref. input/output level for 0 dBm */
		FMTD_Params   *local_ptr,    /* pointer to user defined parameters */
        int           *stack_ptr);

void FMTD_detect(
        FMTD_Handle    fmtd_ptr);    /* Func. operating on FMTD obj frame */

void FMTD_fullDuplexDetect(
        FMTD_Handle    fmtdRin_ptr,
        FMTD_Handle    fmtdSout_ptr);   /* Full duplex mode of operation */

#endif
