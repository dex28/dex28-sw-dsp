*******************************************************************************
*
*   C54x Line-Echo Canceller (LEC)
*
*******************************************************************************

                .title "LEC.asm"
                
                .mmregs ; define global symbolic names for the C54x registers 

*******************************************************************************
*
*   Declaration of exported symbols
*
*******************************************************************************

                .def    _LEC_init, _LEC_input
                .def    _V0

*******************************************************************************
*
*   Constants declaration
*
*******************************************************************************

TAPS            .set    512              ; 256 taps = 32 ms; Note: maximum = 512

BLOCKSIZE       .set    1                ; Must be 2^n
                                         ; TAPS/BLOCKSIZE must be integer

BLOCKSIZE2      .set    32               ; Must be 2^n and must be >= BLOCKSIZE
                                         ; TAPS/BLOCKSIZE2 must be integer

STAU            .set    5                ; short-time LPF tau = 4 ms
                                         ; = 125 us * 2^(-STAU)
                                        
LTAU            .set    8                ; long-time LPF tau = 16 ms
                                         ; = 125 us * 2^(-LTAU)
                                        
NER             .set    2                ; near-end speech threshold = -6dB
                                         ; = -6dB * NER

GAIN            .set    11               ; beta1 = 2^(-GAIN)

; ---------------------------------------- Random Number Generator

MULT            .set 31821               ; multiplier value (last 3 digits are even-2-1)
INC             .set 13849               ; 1 and 13849 have been tested

*******************************************************************************
*
*   Address Register (AR) usage
*
*******************************************************************************

                .asg    AR2, y_ptr
                .asg    AR3, a_ptr
                .asg    AR4, un_ptr
                .asg    AR5, yn_ptr

*******************************************************************************
*
*   Variable declarations + memory allocation
*
*   [s/u]Q<m>.<n> : fixed-point fractional representation:
*           [s/u] : signed/unsigned; signed consumes one bit, unsigned doesn't
*           <m>   : bits right of fixed point
*           <n>   : bits left of fixed point
*
*******************************************************************************

;   PARAMETERS
                .bss THRES         ,1    ; sQ0.15: residual output suppression threshold
                .bss HANGT         ,1    ; sQ15.0: hangover counter reset value
                .bss LY_CUTOFF     ,1    ; sQ0.15: LY cutoff level for no update

;   CONTROL FLAGS

                .bss freeze        ,1    ; bool: freeze update if != 0

;   COUNTERS

                .bss H             ,1    ; sQ15.0: modulo <BLOCKSIZE> counter
                .bss H2            ,1    ; sQ15.0: modulo <BLOCKSIZE2> counter
                .bss HCNTR         ,1    ; sQ15.0: hangover counter

;   CONTEXT VARIABLES

                .bss Y0_ptr        ,1    ; ptr to sQ15.0: pointer to y(0) in Y_CIRCBUF []
                .bss S0F           ,2,,1 ; sQ15.16: short-time LPF of |s(n)|/2
                .bss Y0F           ,2,,1 ; sQ15.16: short-time LPF |y(n)|
                .bss LY            ,2,,1 ; sQ15.16: long-time LPF |y(n)|
                .bss LU            ,2,,1 ; sQ15.16: long-time LPF |u(n)|
                .bss Max_M         ,1    ; sQ15.0: short-term reference maximum

;   COMFORT NOISE GENERATOR CONTEXT

                .bss RNDNUM        ,1    ; uQ16.0: white noise pseudo-random number
                .bss B0            ,1    ; sQ15.0: 1st LPF of the white noise
                .bss B1            ,1    ; sQ15.0: 2nd LPF of the white noise
                .bss B2            ,1    ; sQ15.0: 3rd LPF of the white noise

;   LOCAL/TEMPORARY VARIABLES

                .bss S0            ,1    ; sQ15.0: input near-end sample
                .bss Y0            ,1    ; sQ15.0: input reference sample
                .bss U0            ,1    ; sQ15.0: canceller output (before suppression)
_V0             .bss V0            ,1    ; DEBUG
                .bss EEST          ,1    ; sQ15.0: output of the transversal filter
                .bss ILY           ,1    ; sQ0.15: 1/LY
                .bss error_mu      ,1    ; sQ0.15: beta1 * UN(0) / LY

;   TABLES AND ARRAYS

                .bss A0            ,TAPS              ; sQ0.15: FIR filter tap weight coefficient
                .bss M0            ,TAPS/BLOCKSIZE2+1 ; sQ15.0: partial reference maxima
                .bss UN0           ,BLOCKSIZE         ; sQ0.15: normalized reference samples

*******************************************************************************
*
*   SAMPLES SECTION
*
*******************************************************************************

                .sect ".samples"
                .align 0x400                 ; for up to 1023 circ. buffer size
;               .align 0x800                 ; for up to 2047 circ. buffer size
;
;   Circular buffer of reference samples in sQ15.0 format.
;   Buffer should be in separate section because of alignment needed
;   for circular buffer addressing. Section should be aligned on boundary 
;   depending of number of TAPS.
;
                .if BLOCKSIZE > 1 
Y_CIRCBUF       .space (TAPS+BLOCKSIZE)*16   ; sQ15.0: reference samples
                .else ; NLMS
Y_CIRCBUF       .space (TAPS+BLOCKSIZE)*2*16 ; sQ15.0: reference + normalized samples
                .endif

*******************************************************************************
*
*   CODE SECTION
*
*******************************************************************************

                .text
*******************************************************************************
*
*   Function: LEC_init. LEC variable initialization.
*
*       extern "C" 
*       void LEC_init
*       ( 
*           void
*       );
*
*   AR register usage:
*
*       AR0   -- as general pointer
*
*   Note: C/C++ "Save On Entry" registers are: AR1, AR6, AR7 and SP
*
*******************************************************************************

_LEC_init:
;
;       Clear arrays
;
                STM   #A0, AR0                   ; clear A()
                RPTZ  A, #TAPS
                STL   A, *AR0+

                STM   #Y_CIRCBUF, AR0            ; clear Y()
                .if BLOCKSIZE > 1
                RPTZ  A, #(TAPS+BLOCKSIZE)
                .else ; NLMS
                RPTZ  A, #(TAPS+BLOCKSIZE)*2
                .endif
                STL   A, *AR0+

                STM   #UN0, AR0                  ; clear UN()
                RPTZ  A, #BLOCKSIZE
                STL   A, *AR0+
                
                STM   #M0, AR0                   ; clear M()
                RPTZ  A, #(TAPS/BLOCKSIZE2+1)
                STL   A, *AR0+
;
;       Load default values
;
                
                ST    #Y_CIRCBUF, *(Y0_ptr)      ; points to circular buffer of reference samples
                
                ST    #0x0800, *(THRES)          ; Residual echo suppressor threshold = 1/16 (-24dB)
                                                 ; = sQ0.15 value (-6dB * 0x8000 / value)

                ST    #0x0021, *(LY_CUTOFF)      ; LY cut-off for no update = -48dB
                                                 ; = -6dB * (13 - LN(value))

                ST    #8 * 192, *(HANGT)         ; Near-end speech hang-over time = 192 ms
                                                 ; = 125us * value
                
                ST    #0x0021, *(Max_M)          ; Max_M set to this level to prevent near-end speech
                                                 ; from being declared on startup.
                LD    #0x0400, 16, A
                
                DST   A, *(Y0F)                  ; reference input short-time power estimate initial value
                
                DST   A, *(LY)                   ; reference input long-time power estimate initial value

                LD    #0, A

                DST   A, *(LU)                   ; canceller output long-time power estimate initial value
                
                DST   A, *(S0F)                  ; near-end input short-time power estimate initial value

                STL   A, *(HCNTR)                ; HCNTR = 0
                STL   A, *(H)                    ; H = 0
                STL   A, *(H2)                   ; H2 = 0

                ST    #21845, *(RNDNUM)          ; R.N.G. seed
                ST    #0, *(B0)                  ; pink noise LPF init value
                ST    #0, *(B1)                  ; pink noise LPF init value
                ST    #0, *(B2)                  ; pink noise LPF init value

                RET

*******************************************************************************
*
*   Function: LEC_input. Get new samples and do LEC. (avg. 806 cycles/sample)
*
*       extern "C" 
*       short LEC_input
*       ( 
*           short Y, // reference sample
*           short S, // near-end sample 
*       );
*
*   Returns:
*
*       output sample
*
*   AR register usage:
*
*       AR0   -- as pointer
*       AR2   -- as un_ptr
*       AR3   -- as a_ptr
*       AR4   -- as y_ptr
*
*   Note: C/C++ "Save On Entry" registers are: AR1, AR6, AR7 and SP
*
*******************************************************************************

_LEC_input:
                LD    1h, B
                STL   A, *(Y0)                   ; First argument: reference sample
                STL   B, *(S0)                   ; Second argument: near-end sample
;
;       Setup ALU context
;
                SSBX  OVM                        ; Enable overflow saturation mode
                SSBX  SXM                        ; Enable SXM sign extension mode
                SSBX  FRCT                       ; Fractional mode ON

*******************************************************************************
*
*   Input reference sample into Y_CIRCBUF [] circular buffer.
*   If NLMS, calculate normalized sample yn(n) and store it also in Y_CIRCBUF
*
*******************************************************************************
;
;       y_ptr = circular( ++y0_ptr );
;       *y_ptr = Y0;
;
;       if ( NLMS )
;          y_ptr = circular( ++y0_ptr );
;          *y_ptr = Y0 * ILY;
;
;       After this, Y0_ptr points to yn(n), Y0_ptr-1 to y(n),
;                   Y0_ptr-2 to yn(n-1), Y0_ptr-3 to y(n-1) etc.
;
     .if BLOCKSIZE > 1

                STM   #(TAPS+BLOCKSIZE), BK      ; setup circular buffer size
                MVDK  *(Y0_ptr), y_ptr           ; y_ptr = Y0_ptr
                MAR   *y_ptr+%                   ; circular( y_ptr++ )

                LD    *(Y0), A                   ; store new sample into reference buffer.
                STL   A, *y_ptr                  ; *y_ptr = Y0

                MVMD  y_ptr, *(Y0_ptr)           ; remember location of y(0)

     .else ; NLMS

                STM   #(TAPS+BLOCKSIZE)*2, BK    ; setup circular buffer size
                MVDK  *(Y0_ptr), y_ptr           ; y_ptr = Y0_ptr
                MAR   *y_ptr+%                   ; circular( y_ptr++ )

                LD    *(Y0), A                   ; store new sample into reference buffer.
                STL   A, *y_ptr+%                ; *y_ptr = Y0

                LD    *(Y0), T                   ; T = Y0; Y0 is sQ15.0 and ILY is sQ0.15
                MPY   *(ILY), A                  ; A = T * ILY, i.e. A = Y0 / LY; A is sQ15.16
                SFTA  A, 15-3                    ; switch sQ15.16 -> sQ0.31 format FIXME sQ3.28
                SAT   A                          ; saturate A to +/- 1.0
                STH   A, *y_ptr                  ; YN(0) = A; get upper sQ0.15 from sQ0.31
                
                MVMD  y_ptr, *(Y0_ptr)           ; remember location of y(0)
     .endif
    
*******************************************************************************
*
*   Calculate short-time power estimate of s(n): S0F, using IIR LPF of |s(n)|
*
*       s~(i+1) = ( 1 - alpha ) * s~(i) + alpha * |s(i)|
*
*       alpha = 2^(-STAU), T = 125 us / alpha
*
*******************************************************************************
;
;       S0F = S0F - S0F * 2^(-STAU) + |S0| * 2^(16-STAU)
;
                LD    *(S0),A                    ; A = S0
                ABS   A, B                       ; B = |A|
                DLD   *(S0F), A                  ; A = S0F
                SUB   A, -STAU, A                ; A = A - A * 2^(-STAU)
                ADD   B, 16-STAU, A              ; A = A + |S0| * 2^(16-STAU)
                DST   A, *(S0F)                  ; S0F = A

*******************************************************************************
*
*   Calculate short-time power estimate of y(n): Y0F, using IIR LPF of |y(n)|
*
*       y~(i+1) = ( 1 - alpha ) * y~(i) + alpha * |y(i)|
*
*       alpha = 2^(-STAU), T = 125 us / alpha
*
*******************************************************************************
;
;       Y0F = Y0F - Y0F * 2^(-STAU) + |Y0| * 2^(16-STAU)
;
                LD    *(Y0), A                   ; A = Y0
                ABS   A, B                       ; B = |A| i.e. B = |Y0|
                DLD   *(Y0F), A                  ; A = Y0F
                SUB   A, -STAU, A                ; A = A - A * 2^(-STAU)
                ADD   B, 16-STAU, A              ; A = A + |Y0| * 2^(16-STAU)
                STH   A, *(Y0F)                  ; Y0F = A

*******************************************************************************
*
*   Calculate long-time power estimate of y(n): LY, using IIR LPF of |y(n)|
*
*       Ly~(i+1) = ( 1 - rho ) * Ly~(i) + rho * ( |y(i)| + cutoff )
*
*       rho = 2^(-LTAU), T = 125 us / rho
*
*******************************************************************************
;
;       Context: B contains |Y0|
;
;       LY = LY - LY * 2^(-LTAU) + |Y0| * 2^(16-LTAU) + LY_CUTOFF * 2^(16-LTAU)
;
                DLD   *(LY), A                   ; A = LY
                SUB   A, -LTAU, A                ; A = A - LY * 2^(-LTAU)
                ADD   B, 16-LTAU, A              ; A = A + |Y0| * 2^(16-LTAU)
                DST   A, *(LY)                   ; LY = A
;
;       Compute ILY = 1/LY
;
                SUB   #0x0021, 16, A             ; A = LY - minimum allowed LY
                BCD   $store_ILY, ALEQ           ; if LY < minimum, (delayed) store max ILY
                LD    #0x0400, A                 ; A = maximum allowed ILY
                
                STM   #LY, AR0                   ; AR0 points to MSB of LY
                LD    #1, 16, A                  ; A = 0x10000
                RPT   #14                        ; Loop 15 times:
                SUBC  *AR0, A                    ; A = A - ( LY << 15 );
                                                 ; if ( A >= 0 ) A = ( A << 1 ) + 1;
                                                 ; else A = ( A << 1 );
$store_ILY:     STL   A, *(ILY)                  ; ILY = A

*******************************************************************************
*
*   Determine if speech is present at the near-end.
*
*   Speech detection (modified Giegel) algorithm:
*
*       if
*           s~(i) >= 2^(-NER) * Max{ y~(i),y~(i-1),... y~(i-TAPS) }
*       then
*           start hangover counter, i.e. declare near end speech
*           freeze tap updates
*       else if hangover counter > 0
*           decrement hangover counter
*       else
*           freeze/enable updates depending on Ly vs. cutoff
*
*******************************************************************************
;
;       Update modulo BLOCKSIZE2 counter H2:
;
;       H2 = ( H2 + 1 ) % BLOCKSIZE2
;
                LD    *(H2), A                   ; A = H2
                ADD   #1, A                      ; A += 1
                AND   #BLOCKSIZE2-1, A           ; A &= BLOCKSIZE2
                BCD   $dont_DMOV_Ms, AGT         ; if H > 0 then goto $dont_DMOV_Ms
                STL   A, *(H2)                   ; (delayed) H2 = A
;
;       whenever H2 == 0
;       Update M's & Max_M every BLOCKSIZE2 samples
;
                STM   #(M0+TAPS/BLOCKSIZE2-1), AR0 ; AR0 = last M - 1
                STM   #(TAPS/BLOCKSIZE2-1), BRC   ; loop TAPS / BLOCKSIZE2 times
                RPTBD $find_largest_M-1
                LD    #0, A                      ; (delayed) A = 0
                LD    A, B                       ; (delayed) B = A
                
                LD    *AR0, A                    ; A = *AR0
                MAX   B                          ; B = max{ A, B }
                DELAY *AR0-                      ; AR0[ 1 ] = *AR0--
$find_largest_M:

                LD    *(Y0F), A                  ; A = Y0F
                STL   A, *(M0)                   ; M0 = Y0F
                MAX   B                          ; A = Max{ A, B }
                BD    $skip_dont_DMOV_Ms         ; (delayed) Now, goto 'end if'
                STL   B, *(Max_M)                ; Max_M = A, i.e. Max_M = Max{ M0, Max_M }

$dont_DMOV_Ms:
;
;       when H2 <> 0:
;       Update most recent partial maximum (M0) and max maximum (Max_M)
;       M0 = Max{ Y0F, M0 }
;       Max_M = Max{ M0, Max_M }
;
                LD    *(Y0F), A                  ; A = Y0F
                LD    *(M0), B                   ; B = M0
                MAX   A                          ; A = Max{ A, B }
                STL   A, *(M0)                   ; M0 = A, i.e. M0 = Max{ Y0F, M0 }
                LD    *(Max_M), B                ; B = Max_M
                MAX   A                          ; A = Max{ A, B }
                STL   A, *(Max_M)                ; Max_M = A, i.e. Max_M = Max{ M0, Max_M }

$skip_dont_DMOV_Ms:
;
;       If near-end short-time signal power estimete (S0F) is greater than
;       the greatest of the partial maxima (Max_M) then declare that near-end speech
;       is detected, load hangover counter (HCNTR) and freeze tap updates.
;
;       If HCNTR > 0, then countdown hangover counter (HCNTR) to zero. Otherwise,
;       if long-time power estimate of y(n) (LY) is below cutoff level (LY_CUTOFF), 
;       then set FREEZE_UPDATE and skip updates of tap weight coefficients a(k).
;
;       if ( S0F > Max_M * 2^(-NER) )
;       {
;           HCNTR = HANGT;
;           freeze = true;
;       }
;       else if ( HCNTR != 0 )
;           --HCNTR;
;       else
;           freeze = ( LY <= LY_CUTOFF );
;
                LD    *(S0F), A                  ; A = S0F
                SUB   *(Max_M), -NER, A          ; A = A - Max_M * 2^(-NER)
                BC    $check_HCNTR, ALEQ         ; If S0F <= Max_M, goto $check_HCNTR

                ST    #1, *(freeze)              ; freeze = true
                
                LD    *(HANGT), A                ; A = HANGT
                BD    $skip_cutoff_check         ; (delayed) skip cutoff check
                STL   A, *(HCNTR)                ; HCNTR = A

$check_HCNTR:
                LD    *(HCNTR), A                ; A = HCNTR
                BC    $check_cutoff, AEQ         ; if HCNTR == 0 goto $check_cutoff
                SUB   #1, A                      ; --A
                BD    $skip_cutoff_check         ; 
                STL   A, *(HCNTR)                ; (delayed) HCNTR = A

$check_cutoff:
                LD    *(LY), A                   ; A = MSB of LY
                SUB   *(LY_CUTOFF), A            ; A = A - LY_CUTOFF
                
                ST    #0, *(freeze)              ; freeze = false
                
                BC    $skip_cutoff_check, AGT    ; if ( LY > LY_CUTOFF ) goto $skip_cutoff_check
                ST    #1, *(freeze)              ; else freeze = true

$skip_cutoff_check:

*******************************************************************************
*
*   Estimate echo: EEST = conv{ y(n), a(n) }
*
*   Convolve reference samples with tap weight coefficients of the filter:
*       _
*       r(i) =  sum{ a(k) * y(i-k) }
*             k=0,N-1
*
*******************************************************************************

    .if BLOCKSIZE > 1

                MVDK  *(Y0_ptr), y_ptr           ; y_ptr = Y0_ptr
                STM   #-1, AR0                   ; step = -1
                STM   #A0, a_ptr                 ; a_ptr = &a_k(0)

                RPTZ  A, #(TAPS-2)               ; A = 0, loop TAPS-1 times:
                MAC   *y_ptr+0%, *a_ptr+, A      ; A += y[] * a[], circular(y_ptr--), a_ptr++

                MACR  *y_ptr, *a_ptr, A          ; last sumation with rounding

                STH   A, *(EEST)                 ; EEST = sum

*******************************************************************************
*
*   Estimate echo: EEST = conv{ y(n), a(n) }
*   and adapt tap weight coefficients in the same loop.
*
*******************************************************************************

    .else ; NLMS

                BITF  *(freeze), #0xFFFF         ; test any bit of freeze

                STM   #error_mu, un_ptr          ; un_ptr = &error_mu
                LD    *(UN0), -(GAIN-3), A       ; A = UN0 * 2^-GAIN * 2^3 (YN is sQ3.12)
                STL   A, *un_ptr                 ; error_mu = A
                XC    2, TC                      ; if a_k update freezed
                ST    #0, *un_ptr                ; then error_mu = 0
                
                LD    #0, ASM                    ; ASM = 0

                STM   #A0, a_ptr                 ; a_ptr = &a(0)
                
                STM   #-1, AR0                   ; step = -1
                MVDK  *(Y0_ptr), y_ptr           ; y_ptr = Y0_ptr
                MAR   *y_ptr+0%                  ; y_ptr = Y0_ptr - 1 = &y(n)
                
                STM   #-2, AR0                   ; step = -2
                MVDK  *(Y0_ptr), yn_ptr          ; yn_ptr = Y0_ptr
                MAR   *yn_ptr+0%                 ; yn_ptr = Y0_ptr - 2 = &yn(n-1)
                
                STM   #(TAPS-1), BRC             ; loop TAPS times
                RPTBD $NLMS_end-1
                LD    #0, B                      ; B = 0
                MPY   *un_ptr, *yn_ptr+0%, A     ; T = error_mu, A = T * yn = error_mu * yn
                                                 ; circ( yn_ptr -= 2 )
                LMS   *a_ptr, *y_ptr+0%          ; A += *a_ptr * 2^16 + 2^15
                                                 ; B += *a_ptr * *y_ptr, circ( y_ptr -= 2 )
                ST    A, *a_ptr+                 ; *a_ptr = A * 2^-16, a_ptr++
             || MPY   *yn_ptr+0%, A              ; A = T * yn = error_mu * yn
                                                 ; circ( yn_ptr -= 2 )
$NLMS_end:
                RND   B                          ; B += 2^-15
                STH   B, *(EEST)                 ; EEST = B / 2^16
                
                LD    *(UN0), A
                STL   A, *(V0)
    .endif

*******************************************************************************
*
*   Compute the output U0
*                     _
*       u(i) = s(i) - r(i)
*
*******************************************************************************
;
;       U0 = S0 - EEST
;
                LD    *(S0), A                   ; A = S0; get near-end sample
                SUB   *(EEST), A                 ; A -= EEST; subtract echo estimate
                STL   A, *(U0)                   ; U0 = A; save output for UN(0)

*******************************************************************************
*
*   Compute normalized output UN(0)
*
*       un(i) = u(i) / Ly
*       if ( un(i) > 1.0 ) un(i) = 1.0
*       if ( un(0) < -1.0 ) un(i) = -1.0
*
*******************************************************************************
;
;       UN0 = Saturated( ( U0 * ILY ) << 16 ) >> 16
;
                LD    *(U0), T                   ; T = U0; U0 is sQ15.0, ILY is sQ0.15
                MPY   *(ILY), A                  ; A = T * ILY, i.e. A = U0 / LY; A is sQ15.16
                SFTA  A, 15                      ; switch sQ15.16 -> sQ0.31 format
                SAT   A                          ; saturate A to +/- 1.0
                STH   A, *(UN0)                  ; UN(0) = A; get upper sQ0.15 from sQ0.31

*******************************************************************************
*
*   Calculate long-time power estimate of u(n) using IIR LPF:
*
*       Lu(i+1) = ( 1 - rho ) * Lu(i) + rho * |u(i)|
*
*       rho = 2^(-LTAU), T = 125 us / rho
*
*******************************************************************************
;
;       Context: A contains U0
;
;       LU = LU - LU * 2^(-LTAU) + |U0| * 2^(16-LTAU)
;
                ABS   A, B                       ; B = |U0|
                DLD   *(LU),A                    ; A = LU
                SUB   A, -LTAU, A                ; A = A - LU * 2^(-LTAU)
                ADD   B, 16-LTAU, A              ; A = A + B * 2^(16-LTAU)
                DST   A, *(LU)                   ; LU = A

    .if BLOCKSIZE > 1
    
*******************************************************************************
*
*    Adapt tap weight coefficients using the normalized block-update algorithm
*
*       a_k(i+1) = a_k(i) + Gamma(i) * beta,
*
*       where k = H + j * BLOCKSIZE, j = 0..TAPS/BLOCKSIZE-1
*
*       Gamma(i) = ( Sum Y[] * UN[] ) * ILY
*
*******************************************************************************

                BITF  *(freeze), #0xFFFF         ; check any bit
                BC    $skip_update, TC           ; if tap update freezed, skip update.
;
;       H = ( H + 1 ) % 16
;
                LD    *(H), A                    ; A = H
                ADD   #1, A                      ; A += 1
                AND   #BLOCKSIZE-1, A            ; A &= BLOCKSIZE-1
                STL   A, *(H)                    ; H = A
;        
;       Make y_ptr to point to appropriate reference sample block.
;
;       y_ptr = circular( Y0_ptr - H )
;
                MVDK  *(Y0_ptr), y_ptr           ; y_ptr = Y0_ptr
                MVDK  *(H), AR0                  ; AR0 = H
                MAR   *y_ptr-0%                  ; circular( y_ptr -= H )
;
;       Make a_ptr to point to appropriate filter block.
;
;       a_ptr = &a_k(0) + H - BLOCKSIZE; // -BLOCKSIZE because later we preincrement a_ptr += BLOCKSIZE
;
                STM   #A0, a_ptr                 ; a_ptr = &a_k(0)
                MAR   *a_ptr+0                   ; a_ptr += AR0; AR0 == H already
                MAR   *+a_ptr(-BLOCKSIZE)        ; a_ptr -= BLOCKSIZE, i.e a_ptr = &a_k(0) + H - BLOCKSIZE
;
;       Update a_k()
;
                STM   #-1, AR0                   ; AR0 = -1

                STM   #(TAPS/BLOCKSIZE-1), BRC
                RPTB  $block_end-1               ; loop block TAPS/BLOCKSIZE times

                    ; --------------------------   Calculate Sum Y[] * UN[]

                    STM   #UN0, un_ptr           ; un_ptr = &UN(0)

                    RPTZ  A, #(BLOCKSIZE-2)      ; loop BLOCKSIZE-1 times
                    MAC   *y_ptr+0%, *un_ptr+, A ; sum Y(k) * UN(k); Y(k) is sQ15.0, UN(k) is sQ0.15

                    MACR  *y_ptr+0%, *un_ptr, A  ; last sumation with rounding; result sQ15.16

                    MPYA  *(ILY)                 ; T = ILY, B = A(32-16) * T; equivalent to:
                                                 ; B is sQ15.16, A is sQ15.0, ILY is sQ0.15
                    ; --------------------------
                    
                    SFTA  B, 15-GAIN             ; B = B << 15, i.e. sQ15.16 -> sQ0.31 format; and at once
                                                 ; B = B * 2^(-GAIN)

                    ADD   *+a_ptr(BLOCKSIZE), 16, B  ; k += BLOCKSIZE, B = B + a_k(i) * 2^16
                    STH   B, *a_ptr              ; a_k(i) = B / 2^16
$block_end:
$skip_update:
;
;       Delay UN(i), i.e. UN(k+1) = UN(k) for k = BLOCKSIZE-2,..,0
;
                STM   #(UN0+BLOCKSIZE-2), un_ptr ; un_ptr = &UN(BLOCKSIZE-2)
                RPT   #(BLOCKSIZE-2)             ; k = 14,13....0
                DELAY *un_ptr-                   ; UN(k+1) = UN(k)
    .endif                
                
*******************************************************************************
*
*   Suppress the error signal, if necessary, and output sample.
*
*   Algorithm:
*
*       if ( near end speech ) // residual echo suppression is disabled
*           output = U0
*       else if ( LU/LY > threshold )
*           output = U0	
*       else
*           output = comfort noise
*
*******************************************************************************

;
;       Preload OUTPUT with U0 and deamplify it if there is no near-end speech
;
                LD    *(HCNTR), A                ; A = HCNTR
;                NOP                              ; XC latency NOP
;                NOP                              ; latency NOP
                LD    *(U0), B                   ; OUTPUT = U0
                XC    1, AEQ                     ; if ( HCNTR == 0 ), i.e. no near-end speech
                SFTA  B, 0, B                    ; then OUTPUT = OUTPUT / 2^6; reduce OUTPUT 36dB
;
;       If HCNTR > 0 then it is near-end speech (suppressor is disabled), goto $output_done.
;
                LD    *(HCNTR), A                ; A = HCNTR
                BC    $output_done, AGT          ; if ( HCNTR > 0 ) goto $output_done
;
;       If ( LU/LY <= THRES ) suppress the output U0 signal, otherwise goto $output_done.
;
                LD    *(ILY), T                  ; T = 1 / LY
                MPY   *(LU), A                   ; A = LU * T
                SUB   *(THRES), A                ; A -= THRES i.e. A = LU/LY - THRES
                BC    $output_done, AGT          ; if ( LU/LY > threshold ) goto $output_done
  .if 1==1
                LD    #0, B
  .else
;
;       Comfort noise generation
;       ------------------------
;       Comfort noise (i.e. pink noise) is generated as a sum of outputs of low-pass filters
;       of white noise. White noise is output of the 16-bit pseudo-random number generator.
;
                RSBX  OVM                        ; Disable overflow saturation mode
                LD    #INC, A                    ; A = INC
                MAC   *(RNDNUM), #MULT, A, A     ; A = RNDNUM * MULT + INC
                STL   A, -1, *(RNDNUM)           ; RNDNUM = A mod 65536; -1 because of FRCT
                SSBX  OVM                        ; Enable overflow saturation mode
;
;       LPF fc = 4 Hz, A = +10 dB
;
                MPY   *(B0), #32670, A           ; A = B0 * const
                MAC   *(RNDNUM), #(32768-32670)*3, A, A  ; A = A + RNDNUM * (1 - const)
                STH   A, *(B0)                   ; B0 = A
;
;       LPF fc = 48 Hz, A = 0 dB
;
                MPY   *(B1), #31556, A           ; A = B1 * const
                MAC   *(RNDNUM), #(32768-31556), A, A  ; A = A + RNDNUM * (1 - const)
                STH   A, *(B1)                   ; B1 = A
;
;       LPF fc = 716 Hz, A = -10 dB
;
                MPY   *(B2), #18677, A           ; A = B2 * const
                MAC   *(RNDNUM), #(32768-18677)/3, A, A  ; A = A + RNDNUM * (1 - const)
                STH   A, *(B2)                   ; B2 = A
;
;       pink = sum of LPFs of white noise
;
                LD    *(B0), A
                ADD   *(B1), A
                ADD   *(B2), A                   ; A is at normalized (0dB max)
                SFTA  A, -7, B                   ; Reduce to -42dB = -7 * 6dB
  .endif
$output_done:
;
;       Output sample.
;
                LD    B, A                       ; function returns OUTPUT
;
;       Make C/C++ runtime environment happy.
;
                RSBX  OVM                        ; Disable overflow saturation mode
                RSBX  SXM                        ; Disable SXM sign extension mode
                RSBX  FRCT                       ; Fractional mode OFF
                
                RET

*******************************************************************************

                .end
