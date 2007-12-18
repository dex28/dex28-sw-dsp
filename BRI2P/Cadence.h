#ifndef _CADENCE_H_INCLUDED
#define _CADENCE_H_INCLUDED

class Timer
{
	unsigned int reload_value;
	volatile unsigned int count;
	volatile bool event_arrived;
	
public:

	Timer( void ) 
	{
		count = reload_value = 0; 
		event_arrived = false;
		}
	
	void Decrement( void ) // should be called inside TINT irq handler
	{
		if ( count == 0 )
			return;

		if ( --count == 0 )
		{
			count = reload_value;
			event_arrived = true;
			}
		}
	
	void SetOnceTimeout( unsigned int c )
	{
		DisableIRQs ();
		reload_value = 0;
		count = c;
		event_arrived = false;
		EnableIRQs ();
		}
		
	void SetPeriodicTimeout( unsigned int c )
	{
		DisableIRQs ();
		reload_value = c;
		count = reload_value;
		event_arrived = false;
		EnableIRQs ();
		}

	void WaitCountElapse( int value )
	{
	    for ( ; value > 0; value-- )
	    {
	    	unsigned short x = count;
	        do; while( x == count ); // wait one count to elapse
	        }
		}
		
	unsigned short GetCount( void )
	{
		return count;
		}

	bool IsTimeoutArrived( void )
	{
		bool rc = event_arrived;
		if ( rc )
			event_arrived = false;
		return rc;
		}
	};
	
class Cadence
{
	int N; // 0: disabled, -1: endless cadence, > 0 countdown cadences
	unsigned int state; // cadence interval 0..3; even means ON, odd means OFF
	unsigned int T[ 4 ]; // interval duration (in no of calls of Increment())
	unsigned int count; // interval countdown
/*
	Note:
	
	One cadence looks like:
	
    .-------.        .-------.        
    |       |        |       |        
    '       '--------'       '--------
	   T[0]    T[1]     T[2]    T[3]
    <-------------------------------->  == Cadence = sequence of intervals
    
    N is number of cadences:
    	N == -1 means endless
    	N == 0 means disabled
    	
    If some T[] is 0, then cadence stops there.
    
    E.g.
    	T[0] = 10, T[1] = 0, N = 1
    	means one shot of 10ms ON
*/	

public:

	void TurnOff( void )
	{
		state = 1; // freeze in state 1
		count = 0; // disabled interval counter
		N = 0; // disable cadence counter
		}

	void TurnOn( int duration = -1 ) 
	{
		if ( duration < 0 )
		{
			state = 0; // freeze in state 0
			count = 0; // disabled interval counter
			N = 0; // disable cadence counter
			}
		else
		{
			T[ 0 ] = duration; T[ 1 ] = 0;
			state = 0; count = T[ 0 ];
			N = 1; // one cadence
			}
		}

	void SetCadence( int n, int t1, int t2 = 0, int t3 = 0, int t4 = 0 )
	{
		T[ 0 ] = t1; T[ 1 ] = t2;
		T[ 2 ] = t3; T[ 3 ] = t4;
 		state = 0; // initial state is ON
		count = T[ 0 ]; // interval duration countdown
		N = n; // cadence counter
		}

	bool Increment( void )
	{
		if ( N == 0 || count == 0 )
			return true;
			
		if ( --count > 0 )
			return true;
			
		state++;

		if ( state < 4 && T[ state ] > 0 )
		{
			count = T[ state ]; // go to next interval in cadence
			return true;
			}

		state = 0; // start new caddence

		if ( N == -1 ) // endless cadence
			count = T[ state  ];
		else if ( --N > 0 )
			count = T[ state ]; // next period
		else
		{
			state = 1; // OFF
			return false;
			}

		return true;
		}

	unsigned int GetState( void ) const
	{
		return state & 0x01; // returns 0 == ON, 1 == OFF
		}
	};

#endif // _ELU28_H_INCLUDED
