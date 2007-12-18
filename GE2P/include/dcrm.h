/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES CONFIDENTIAL AND
 * PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2005 D2 TECHNOLOGIES ALL RIGHTS RESERVED"
 *
 * THIS IS AN UNPUBLISHED WORK CONTAINING VIRATA CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2002 VIRATA ALL RIGHTS RESERVED"
 *
 * ======== +RCSfile: dcrm.h + ========
 *
 * "@(#) DCRM  +ProjectRevision: 54.3.1.6 +"
 *
 *  This is the DC remove module
 *
 *  Version: +ProjectRevision: 54.3.1.6 +
 *  Performance:
 *      Program(.text) Table(.data) Total  Object Size  MIPS
 *      89             0            89     7            0.1
 *
 */

#ifndef _DCRM
#define _DCRM

/*
*  The following constants define the sizes for the stack space and any
*  required aligned memory sections.
*/
#define DCRM_STACKMEMSIZE (3)

/*
*  The following structure contains the local parameters used by DCRM.
*/
typedef struct {
    int    tBLOCKSIZE;    /* 1, number of samples to process per call */
} DCRM_LParams;

/*
*  Define the min, max, and default values for the local parameters.
*/
#define DCRM_TBLOCKSIZE_MIN (1)
#define DCRM_TBLOCKSIZE_MAX (32767)
#define DCRM_TBLOCKSIZE_DEF (40)

/*
*  The following are offsets into the DCRM_OBJ.
*/
typedef struct {
    int    internal[5];
} _DCRM_Internal;

typedef struct {
    _DCRM_Internal internal;
    int *src;                    /* 6 */
    int *dst;                    /* 7 */
} DCRM_Obj;
typedef DCRM_Obj *DCRM_Handle;



/* Prototypes for all DC remove related functions appear here */
void DCRM_init(DCRM_Handle, DCRM_LParams *, int *);
void DCRM_run(DCRM_Handle);
#endif

