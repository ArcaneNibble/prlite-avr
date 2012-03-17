#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "lib485net.h"
#include "bl_support.h"

#define SLOWDOWN_FACTOR	10

typedef signed long FIXED1616;

typedef struct
{
	FIXED1616 p;
	FIXED1616 i;
	FIXED1616 d;
	signed char orientation;
} __attribute__((__packed__)) pid_gains_packet;

typedef struct
{
	signed int speed;
} __attribute__((__packed__)) setpoints_packet;

typedef struct
{
	unsigned long interval_count;
	signed int speed;

	FIXED1616 debug_p;
	FIXED1616 debug_i;
	FIXED1616 debug_d;
	unsigned int out;
	unsigned int time;
} __attribute__((__packed__)) wheel_status_packet;

inline FIXED1616 int_to_fixed(int i)
{
	return ((FIXED1616)i) << 16;
}
inline int fixed_to_int(FIXED1616 i)
{
	return i >> 16;
}

FIXED1616 fixed_mult(FIXED1616 inArg0, FIXED1616 inArg1)
{
	register unsigned long _a = (inArg0 >= 0) ? inArg0 : (-inArg0);
	register unsigned long _b = (inArg1 >= 0) ? inArg1 : (-inArg1);
	unsigned char needsflipsign = (inArg0 >= 0) != (inArg1 >= 0);
	
	register FIXED1616 result;
	
	asm volatile(
		"eor %A[result], %A[result]"								"\n\t"
		"eor %B[result], %B[result]"								"\n\t"
		"eor %C[result], %C[result]"								"\n\t"
		"eor %D[result], %D[result]"								"\n\t"
		"eor r18, r18"												"\n\t"
		"eor r19, r19"												"\n\t"
		"eor r20, r20"												"\n\t"
		"eor r21, r21"												"\n\t"
		"eor r22, r22"												"\n\t"
		
		// i = 5
		"mul %C[argA], %D[argB]"									"\n\t"
		"mov %A[result], r0"										"\n\t"
		"mov %B[result], r1"										"\n\t"
		"mul %D[argA], %C[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mov %D[result], %C[result]"								"\n\t"
		"mov %C[result], %B[result]"								"\n\t"
		"mov %B[result], %A[result]"								"\n\t"
		"mov %A[result], r22"										"\n\t"
		
		// i = 4
		"mul %B[argA], %D[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mul %C[argA], %C[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mul %D[argA], %B[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mov %D[result], %C[result]"								"\n\t"
		"mov %C[result], %B[result]"								"\n\t"
		"mov %B[result], %A[result]"								"\n\t"
		"mov %A[result], r22"										"\n\t"
		
		// i = 3
		"mul %A[argA], %D[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mul %B[argA], %C[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mul %C[argA], %B[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mul %D[argA], %A[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mov %D[result], %C[result]"								"\n\t"
		"mov %C[result], %B[result]"								"\n\t"
		"mov %B[result], %A[result]"								"\n\t"
		"mov %A[result], r22"										"\n\t"
		
		// i = 2
		"mul %A[argA], %C[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mul %B[argA], %B[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mul %C[argA], %A[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		
		// i = 1
		"mul %A[argA], %B[argB]"									"\n\t"
		"mov r18, r0"												"\n\t"
		"mov r19, r1"												"\n\t"
		"mul %B[argA], %A[argB]"									"\n\t"
		"add r18, r0"												"\n\t"
		"adc r19, r1"												"\n\t"
		"adc r20, r22"												"\n\t"
		"mov r21, r20"												"\n\t"
		"mov r20, r19"												"\n\t"
		"mov r19, r18"												"\n\t"
		"mov r18, r22"												"\n\t"
		
		// i = 0
		"mul %A[argA], %A[argB]"									"\n\t"
		"add r18, r0"												"\n\t"
		"adc r19, r1"												"\n\t"
		"adc r20, r22"												"\n\t"
		
		//low += 0x8000;
		"ldi r22, 0x80"												"\n\t"
		"add r19, r22"												"\n\t"
		"eor r22, r22"												"\n\t"
		"adc r20, r22"												"\n\t"
		
		//mid += (low >> 16);
		"add %A[result], r20"										"\n\t"
		"adc %B[result], r21"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"adc %D[result], r22"										"\n\t"
		
		"eor r1, r1"												"\n\t"

		: [result] "=&r" (result)
		: [argA] "r" (_a),	[argB] "r" (_b)
		: "r18", "r19", "r20", "r21",		"r22"
	);

	/* Figure out the sign of result */
	if (needsflipsign)
	{
		result = -result;
	}

	return result;
}

FIXED1616 fixed_div(FIXED1616 a, FIXED1616 b)
{
	signed long long tmp;

	tmp = (signed long long)(a);
	tmp *= 65536;
	tmp /= (signed long long)(b);

	return (FIXED1616)tmp;
}

signed int position;
signed char orientation;

/***********************************
** updatePosition
***********************************/
void updatePosition(void)
{	//function either increments or decrements the "position"
	//variable based on the new inputs.  This ISR is executed every time
	//there is a pin change on either sensor pin (PD2, PD3)
	
	signed char pos_adv_val;
	unsigned char newstate;	    //this loads the new state of the two encoder inputs
	static unsigned char oldstate = 0;
	
	//mask away all but the correct two bits
	newstate = ((PIND & 0x0C) >> 2);	//mask = 0000,1100
	
	pos_adv_val = orientation * (signed char)(-1 + (((newstate & 0x01) << 1)^(oldstate & 0x02)));
	position += pos_adv_val;
	//pls_accum[idx_ctr] += pos_adv_val;

	oldstate = newstate;

}

/***********************************
** INT0 interrupt subroutine
***********************************/
ISR(INT0_vect)	//input pin change interrupt INT 0 (on PD2)
{	
	updatePosition();
}

/***********************************
** INT0 interrupt subroutine
***********************************/
ISR(INT1_vect)	//input pin change interrupt INT1 (on PD3)
{	
	updatePosition();
}

int main(void)
{
	FIXED1616 kp=0, ki=0, kd=0;
	signed int setpoint=0;
	signed int numticks;
	void *gains_dgram, *setpoints_dgram, *status_dgram, *reprogram_dgram;
	unsigned long interval = 0;
	signed int oldposition = 0;
	unsigned char packet_buf[64];
	unsigned char packet_len;
	pid_gains_packet *gains;
	setpoints_packet *setpoints;
	wheel_status_packet *status;
	unsigned char delay = SLOWDOWN_FACTOR;
	int speed_intervals = 0;

	initLib();
	setAddr(bl_get_addr());
	
	ICR1 = 40000;	//20 ms with 16 Mhz clock with /8 (2Mhz)
	//valid range for pwm is 2000 (1 ms) to 4000 (2 ms)  idle is 3000
	OCR1A = 3000;
	TCCR1A = _BV(COM1A1) | _BV(WGM11);	//clear on match, reset on rollover
	TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS11);		//mode 14 (reset on icr1)   clock div 8
	//TIMSK1 = _BV(TOIE1);

	DDRB |= _BV(PB1);	//enable pwm port

	DDRD &= ~(_BV(PD2) | _BV(PD3));	//enable encoder input
	//encoder 0		a = pd2 b = pd3
	EICRA = _BV(ISC10) | _BV(ISC00);	//both edges on int0 and int1
	EIMSK = _BV(INT0) | _BV(INT1);	//enable int0 and int1
	
	status_dgram = connectDGram(0xF0, 0, 7);	//pc port 7 is incoming status for everything
	gains_dgram = listenDGram(1);
	setpoints_dgram = listenDGram(2);
	reprogram_dgram = listenDGram(7);
	
	gains = setpoints = status = &(packet_buf[0]);
	
	sei();
	
	while(1)
	{
		if(TIFR1 & _BV(TOV1))
		{
			TIFR1 = _BV(TOV1);
			
			//every 20 ms
			if(recvDGram(gains_dgram, &(packet_buf[0]), &packet_len) == 0)
			{
				//there can be more than one, but not likely and 20 ms won't hurt
				if(packet_len == sizeof(pid_gains_packet))
				{
					kp = gains->p;
					ki = gains->i;
					kd = gains->d;
					orientation = gains->orientation;
					delay = 1;
				}
			}
			
			if(recvDGram(setpoints_dgram, &(packet_buf[0]), &packet_len) == 0)
			{
				if(packet_len == sizeof(setpoints_packet))
				{
					setpoint = setpoints->speed;
					delay = 1;
				}
			}
			
			if(recvDGram(reprogram_dgram, &(packet_buf[0]), &packet_len) == 0)
			{
				if(packet_len == 8)
				{
					if(packet_buf[0] == 0x55 && 
						packet_buf[1] == 0xAA && 
						packet_buf[2] == 0x52 && 
						packet_buf[3] == 0x53 && 
						packet_buf[4] == 0x54 && 
						packet_buf[5] == 0x21 && 
						packet_buf[6] == 0xC3 && 
						packet_buf[7] == 0x3C)
					{
						bl_erase_all_csum();
						bl_reboot();
					}
					
					else if(packet_buf[0] == 0x55 && 
						packet_buf[1] == 0xAA && 
						packet_buf[2] == 0x4C && 
						packet_buf[3] == 0x4D && 
						packet_buf[4] == 0x4E && 
						packet_buf[5] == 0x4F && 
						packet_buf[6] == 0xC3 && 
						packet_buf[7] == 0x3C)
					{
						bl_erase_lib_csum();
						bl_reboot();
					}
					
					else if(packet_buf[0] == 0x55 && 
						packet_buf[1] == 0xAA && 
						packet_buf[2] == 0x41 && 
						packet_buf[3] == 0x42 && 
						packet_buf[4] == 0x43 && 
						packet_buf[5] == 0x44 && 
						packet_buf[6] == 0xC3 && 
						packet_buf[7] == 0x3C)
					{
						bl_erase_app_csum();
						bl_reboot();
					}
				}
			}
			
			speed_intervals++;
			
			if(--delay == 0)
			{
				delay = SLOWDOWN_FACTOR;
				ATOMIC_BLOCK(ATOMIC_FORCEON)
				{
					numticks = position - oldposition;
					oldposition = position;
				}
				
				{
					FIXED1616 errf;
					static FIXED1616 i = 0;
					static FIXED1616 old_err = 0;
					FIXED1616 d;
					FIXED1616 pout, iout, dout, outf;
					signed int newval;
					FIXED1616 currentspeed;
					
					currentspeed = int_to_fixed(numticks * 50);
					currentspeed = fixed_div(currentspeed, int_to_fixed(speed_intervals));
					//if I do not ever cut short the interval, this would simply be numticks*5
					
					errf = int_to_fixed(setpoint) - currentspeed;
					
					i += errf;
					d = errf - old_err;
					old_err = errf;
					
					pout = fixed_mult(errf, kp);
					iout = fixed_mult(errf, ki);
					dout = fixed_mult(errf, kd);
					
					outf = pout + iout + dout;
					
					newval = fixed_to_int(outf);
					newval += 3000;
					
					if(newval > 4000) newval = 4000;
					if(newval < 2000) newval = 2000;
					
					OCR1A = newval;
					
					status->interval_count = interval++;
					status->speed = numticks;
					status->debug_p = pout;
					status->debug_i = iout;
					status->debug_d = dout;
					status->out = newval;
					status->time = TCNT1;
				}
				
				speed_intervals = 0;
				sendDGram(status_dgram, &(packet_buf[0]), sizeof(wheel_status_packet));
			}
		}
	}
}
