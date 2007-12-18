
#include "DSP.h"
#include "MsgBuf.h"
#include "xhfc.h"

///////////////////////////////////////////////////////////////////////////////

/*
        hfc.port[ port_id ].D_TX_Send ();
        hfc.port[ port_id ].D_TX_Abort ();
        hfc.port[ port_id ].D_TX_Query ();
        hfc.port[ port_id ].PH_ActivateRequest ();
        hfc.port[ port_id ].PH_DeactivateRequest ();
        hfc.port[ port_id ].D_TX_Append( tx_octet );
        
        if ( ! hfc.IsIrqAsserted () ) // Poll interrupt pin
            continue;

        if ( ! hfc.InterruptHandler () )
            continue;

        if ( ! hfc.BottomHalf_EH () )
            continue;

*/
