/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2005 D2 TECHNOLOGIES, INC ALL RIGHTS RESERVED"
 *
 * THIS IS AN UNPUBLISHED WORK CONTAINING VIRATA CORPORATION CONFIDENTIAL AND
 * PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2002 VIRATA CORPORATION ALL RIGHTS RESERVED"
 *
 * ======== +RCSfile: g729ab.h + ========
 *
 * "@(#) +ProjectRevision: 54.3.2.7 +"
 *
 *  ======= g729ab.h =======
 *
 *  This is ITU G.729A with Annex B vocoder.
 *
 *  ******** Object file descriptions: ********
 *
 *   Encoder: 
 *      Program(.text): 5962+2540
 *      Table(.data)  : 3175
 *      Total         : 11676
 *
 *   Decoder:
 *      Program(.text): 2075+2540
 *      Table(.data)  : 2696
 *      Total         : 7311
 *
 *   Full Duplex:
 *      Program(.text): 10532(*)
 *      Table(.data)  : 3174(**)
 *      Total         : 13751
 *
 *  (*) : 2540 words are shared by encoder and decoder.
 *  (**): encoder uses all the data.
 *
 * 
 *  ******** Memory Performance ********
 *
 *    Encoder:
 *       Object memory /port: 722
 *       
 *    Decoder:
 *       Object memory /port: 428
 *
 *    Full Duplex:
 *       Object memory /port    : 1150
 *       VP Open stack usage    : G729AB_STACKMEMSIZE
 *       VP Open B1 memory usage: G729AB_B1MEMSIZE
 *
 *  ******** MIPs Performance ********
 *
 *    Encoder:
 *       G729AB_init()  : 2170 cycles
 *       G729AB_encode():
 *         Table(.data) on external zero-wait RAM: 9.71 MIPs 
 *         Table(.data) on internal DARAM        : 9.00 MIPs 
 *       
 *    Decoder:
 *       G729AB_init()  : 1297 cycles
 *       G729AB_decode():
 *         Table(.data) on external zero-wait RAM: 2.24 MIPs 
 *         Table(.data) on internal DARAM        : 2.20 MIPs 
 *
 *    Full Duplex:
 *       Table(.data) on external zero-wait RAM: 11.3 MIPs
 *       Table(.data) on internal DARAM        : 11.1 MIPs
 *
 *  Note1:G729AB_encode() returns 0, 1, or 5, the number of words 
 *        in the output buffer. The encObj.src_ptr points to a data 
 *        buffer of size 80 (10 ms).
 * 
 *        G729AB_decode() returns 0, 1, or 5, the number of words 
 *        in the input buffer. The decObj.src_ptr points to a codeword 
 *        buffer of size 0, 1 or 5. decObj.dst_ptr points to a 
 *        data buffer of size 80 (10 ms).
 *
 *  Note2:AR1 is modified during processing. An ISR cannot use it as the
 *        stack pointer.
 */

#ifndef _G729AB
#define _G729AB

/* 
 * Constant definitions
 */
#define G729AB_FRAME_SIZE   (80) /* G729AB input speech frame size (words) */
#define G729AB_INIT_CWSIZE	(5)  /* G729AB codeword size (words) */
#define G729AB_ENCODE		(0)  /* mode parameter for G729B_init() */
#define G729AB_DECODE		(1)  /* mode parameter for G729B_init() */

/*
 * The following constants define the sizes for the stack space and any
 * required aligned memory sections.
 */
#ifndef VPO_MEM_STRUCTURES
typedef struct {
    int obj;
    int b1Mem;
    int temp;
    int one;               /* DARAM location for 1 (speeds rounding) */
    int minus;
    int max_16;
    int min_16;
    int six;
    int saveStack;         /* temporary storage location for restoring SP */
    int enc_dec_share[54]; /* variables used in main program */
    int share[65];         /* local variables used in subroutines */
    int arrays[1174];     
    int commVar[6];        /* local arrays used in subroutines */
} _G729AB_Stackmem;

/*
 * Note: c1A and c2A are circular buffers of size 10, so
 *       the last 4 bits of address must be 0000.
 */
typedef struct {
    int c1A[10];  /* 10, aligned memory */
    int hole[6];  /* 6, to make c2A aligned */
    int c2A[10];  /* 10, aligned memory */
} _G729AB_B1mem;
#endif

/*
 * The following constant defines the alignment requirements on the B1 memory.
 * The B1 memory starting address must be aligned such that:
 * (B1 memory address) & (MOD_B1MEMALIGN) == 0
 */
#define G729AB_B1MEMALIGN (0x000f)

#ifndef EXPORT

/*
 * total of 423 words for internal decoder object
 */
typedef struct {
    int _stackmem;
    int _b1mem;
    int	_old_T0;          /* 60 */
    int	_sharp;           /* SHARPMIN */
    int	_seed_fer;        /* G729AB 21845 */
    int	_seed;            /* INIT_SEED for Random() */
    int	_past_ftyp;       /* G729AB 1 */
    int	_sh_sid_sav;      /* G729AB 1 */
    int	_sid_sav;         /* G729AB */
    int	_gain_pitch;
    int	_gain_code;
    int	_prev_ma;         /* for D_lsp() */
    int	_y1_lo;           /* for Post_Process() */
    int	_y2_lo;           /* for Post_Process() */
    int	_y1_hi;           /* for Post_Process() */
    int	_y2_hi;           /* for Post_Process() */
    int	_x0;              /* for Post_Process() */
    int	_x1;              /* for Post_Process() */
    int	_mem_syn[10];     /* [_G729A_M] */
    int	_past_qua_en[4];
    int	_lsp_old[10];     /* [_G729A_M] */
    int	_prev_lsp[10];    /* [_G729A_M] for D_lsp() */
    int	_freq_prev[40];   /* [_G729A_MA_NP][_G729A_M] for D_lsp */
    int	_cur_gain;        /* G729AB for Dec_cng() */
    int	_sid_gain;        /* G729AB for Dec_cng() must folow cur_gain */
    int	_lspSiD[10];      /* for Dec_cng() must folow sid_gain */
    int	_old_exc[154];    /* [_G729A_PIT_MAX+_G729A_L_INTERPOL] */
    int	_mem_pre;
    int	_past_gain;       /* initially 4096 */
    int	_mem_syn_res[10]; /* [_G729A_M] must follow past_gain */
    int	_mem_syn_pst[10]; /* [_G729A_M] */
    int	_res2_buf[143];   /* [_G729A_PIT_MAX+] */
} _G729AB_Dinternal;

/* 
 * total 40 words
 */
typedef struct {
    int	_Prev_Min;
    int	_Min;               /* must come after _Prev_Min */
    int	_Next_Min;
    int	_MeanE;
    int	_MeanSE;
    int	_MeanSLE;
    int	_MeanSZC;
    int	_prev_energy;
    int	_count_sil;
    int	_count_update;
    int	_count_ext;
    int	_flag;
    int	_v_flag;
    int	_less_count;
    int	_MeanLSF[10];       /* [_G729A_M] */
    int	_Min_buffer[16];
} _VAD_Str;		

/* 
 * total 104 words
 */
typedef struct {
    int	_sh_RCoeff;	    /* for Cod_cng */
    int	_fr_cur;        /* for Cod_cng */
    int	_cur_gain;      /* for Cod_cng */
    int	_nb_ener;       /* for Cod_cng */
    int	_sid_gain;      /* for Cod_cng */
    int	_flag_chang;    /* for Cod_cng */
    int	_prev_energy;   /* for Cod_cng Must follow flag_chang */
    int	_count_fr0;     /* for Cod_cng */
    int	_sh_Acf[2];     /* [_G729A_NB_CURACF] for Cod_cng*/
    int	_sh_sumAcf[3];  /* [_G729A_NB_SUMACF] Must follow sh_Acf[] */
    int	_sh_ener[2];    /* [_G729A_NB_GAIN] Must follow sh_sumAcf[] */
    int	_ener[2];       /* [_G729A_NB_GAIN]  Must follow sh_ener[] */
    int	_lspSid_q[10];  /* [_G729A_M] for Cod_cng*/
    int	_pastCoeff[11]; /* [_G729A_MP1] for Cod_cng*/
    int	_RCoeff[11];    /* [_G729A_MP1] for Cod_cng*/
    int	_Acf[22];       /* [_G729A_SIZ_ACF] for Cod_cng*/
    int	_sumAcf[33];    /* [_G729A_SIZ_SUMACF] for Cod_cng*/
} _COD_CNG_Str;	

/*
 * total of 719 words for internal encoder object
 */
typedef struct {
    int _stackmem;
    int _b1mem;
    int	_sharp;            /* initially SHARPMIN */
    int	_pastVad;          /* G729AB init to 1 */
    int	_ppastVad;         /* G729AB init to 1 */
    int	_seed;             /* G729AB init to INIT_SEED */
    int	_frm_count;        /* G729AB wraps to 256 */
    int	_mem_w0[10];       /* [M] must come after _frm_count */
    int	_past_qua_en[4];   /* must all be initialized to -14336 */
    int	_lsp_old[10];      /* [M] initialize to lsp_old_table[] */
    int	_lsp_old_q[10];    /* [M] must come after _lsp_old[] */
    int	_L_exc_err[8];     /* [4][2] stored Hi/Lo, used by taming */
    int	_old_A[11];        /* [M+1] [0]=4096, others zeroed, for Levinson() */
    int	_old_rc[2];	       /* G729AB for Levinson() */
    int	_Pre_Proc_y1_hi;
    int	_Pre_Proc_y2_hi;   /* must come after _Pre_Proc_y1_hi */
    int	_Pre_Proc_y1_lo;   /* must come after _Pre_Proc_y2_hi */
    int	_Pre_Proc_y2_lo;   /* must come after _Pre_Proc_y1_lo */
    int	_Pre_Proc_x0;      /* must come after _Pre_Proc_y2_lo */
    int	_Pre_Proc_x1;      /* must come after _Pre_Proc_x0 */
    int	_mem_w[10];        /* [_G729A_M] (needed for wsp) */
    int	_freq_prev[40];    /* [_G729A_MA_NP][_G729A_M] for Qua_lsp */
    int	_old_exc[154];	   /* [L_FRAME+PIT_MAX+L_INTERPOL] */
    int	_old_speech[160];  /* [_G729A_L_TOTAL] */
    int	_old_wsp[143];     /* [_G729A_L_FRAME+_G729A_PIT_MAX] */
    int	_vad_str[40];      /* G729AB VAD */
    int	_cod_cng_str[104]; /* G729AB cod_cng */
} _G729AB_Einternal;

#endif

/* 
 * ======== G729AB decoder object ========
 * Total of 423 + 5 = 428 words for decoder object
 */
typedef struct {
    _G729AB_Dinternal  internal;
    int	               bad_lsf;
    int	               bfi;
    int	               nWords;   /* 0, 1, or 5 words */
    int               *src_ptr;  /* [G729AB_INIT_CWSIZE] */
    int	              *dst_ptr;  /* [G729AB_FRAME_SIZE] */
} G729AB_DObj;

/* 
 * ======== G729AB encoder object ========
 * total of 719 + 3 = 722 words for encoder object
 */
typedef struct {
    _G729AB_Einternal  internal;
    int	               vad_enable;
    int	              *src_ptr;    /* [G729AB_FRAME_SIZE] */
    int	              *dst_ptr;    /* [G729AB_INIT_CWSIZE] */
} G729AB_EObj;

typedef G729AB_EObj *G729AB_EHandle;
typedef G729AB_DObj *G729AB_DHandle;

#ifdef ASMDEFS

#define _G729A_L_FRAME      (80)
#define _G729A_L_TOTAL      (240)
#define _G729A_PIT_MAX      (143)
#define _G729A_M            (10)
#define _G729A_MA_NP        (4)
#define _G729A_L_INTERPOL   (10 + 1)
#define _G729A_PRM_SIZE     (11)
#define _G729A_NB_CURACF    (2)
#define _G729A_SIZ_ACF      (11 * 2)
#define _G729A_NB_SUMACF    (3)
#define _G729A_SIZ_SUMACF   (11 * 3)
#define _G729A_NB_GAIN      (2)

#else

#ifndef EXPORT
/* 
 * define sizes of memory buffers for use with developer C code
 */
#define G729AB_STACKMEMSIZE sizeof(_G729AB_Stackmem)
#define G729AB_B1MEMSIZE    sizeof(_G729AB_B1mem)

#endif              /* ifndef EXPORT */


/* 
 * Functions prototypes
 */
int G729AB_init(
    void *obj_ptr,
    int   mode,
    int  *stack_ptr,
    int  *b1_ptr);

int G729AB_encode(
    G729AB_EHandle enc_ptr);

int G729AB_decode(
    G729AB_DHandle dec_ptr);

#endif
#endif
