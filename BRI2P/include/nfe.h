/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2005 D2 TECHNOLOGIES, INC ALL RIGHTS RESERVED"
 *
 * THIS IS AN UNPUBLISHED WORK CONTAINING VIRATA CORPORATION CONFIDENTIAL AND
 * PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2000 VIRATA CORPORATION ALL RIGHTS RESERVED"
 *
 * ======== +RCSfile: nfe.h + ========
 *
 * "@(#) NFE  +ProjectRevision: 54.3.5.2 +"
 *
 *  ******** Object file descriptions: ********
 *  nfe.o54: contains NFE_init() and NFE_run(), normal C mode.
 *  nfe.f54: contains NFE_init() and NFE_run(), far C mode.
 *    Program(.text):   285 words
 *    Table(.data):      47 words
 *    Total:            332 words
 *
 *  ******** MIPs/Memory Performance ********
 *
 *  NFE_init():
 *      # cycles:                446   
 *      Object memory /port:      47     
 *      C stack usage:           NFE_STACKMEMSIZE
 *      VP Open stack usage:     none
 *      VP Open B1 memory usage: none
 *
 *  NFE_run():
 *                               (xmit_ptr == NULL)     (xmit_ptr != NULL)
 *      5ms block MIPs:          0.120                  0.138
 *      8ms block MIPs:          0.078                  0.092
 *      Object memory /port:     47
 *
 *      C stack usage:           0
 *      VP Open stack usage:     NFE_STACKMEMSIZE
 *      VP Open B1 memory usage: none
 *  
 *  Note: NFE requires COMM functions not included in the above stated
 *        memory performance.
 *
 */


#ifndef _NFE
#define _NFE

#include <comm.h>

/* initialization modes */
#define NFE_COLDSTART       (0)
#define NFE_WARMSTART       (1)

#define NFE_MINNOISE_DEF    (-60)   /* -60 dBm */
#define NFE_MAXNOISE_DEF    (-35)   /* -35 dBm */
#define NFE_BLKSIZE_DEF     ( 40)   /*  40 samples per block  CHANGED FOR 1/2 MS BLKSIZE */

#define NFE_STACKMEMSIZE (7)

/* local parameter definition */
typedef struct {
    int    pMINNOISE;
    int    pMAXNOISE;
    int    pBLKSIZE;                 /* number of sample per block */
} NFE_Params;

typedef struct {
    int    internal[44];
} _NFE_Internal;

typedef struct {
    _NFE_Internal  internal;
    int           *xmit_ptr;   /* 45, transmit buffer pointer */
    int           *src_ptr;    /* 46, input buffer pointer */
    int            noiseFloor; /* 47, noise floor in dbm units*/
} NFE_Obj;

typedef NFE_Obj *NFE_Handle;

void NFE_init(
     NFE_Handle     nfe_ptr, 
     GLOBAL_Params *global_ptr, 
     NFE_Params    *local_ptr,
     int            mode,
     int           *stack_ptr);

void NFE_run(
     NFE_Handle     nfe_ptr);
#endif
