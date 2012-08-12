#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "lib485net.h"
#include "bl_support.h"
#include "avr-fixed.h"

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
	//bit0:	is waiting on sync line
	//bit1: logic level of sync line
	//bit2: PID gains have ever been set
	//bit3: command rx toggle
	//bit4: is no longer holding down sync (got command)
	unsigned char debug_bits;
} __attribute__((__packed__)) wheel_status_packet;

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
	
	void *gains_dgram, *setpoints_dgram, *status_dgram, *jumbo_dgram, *reprogram_dgram, *hack_sync_dgram;
	unsigned char packet_buf[64];
	unsigned char packet_len;
	unsigned char myaddr;
	unsigned char *packet_subset;
	unsigned char packet_subset_len;
	pid_gains_packet *gains;
	setpoints_packet *setpoints;
	pid_gains_packet *gains2;
	setpoints_packet *setpoints2;
	wheel_status_packet *status;
	
	unsigned char waiting_sync = 0;
	signed int new_setpoint = 0;
	
	signed int oldnumticks = 0;
	unsigned long interval = 0;
	signed int oldposition = 0;
	unsigned char wait_cnt = 0;
	FIXED1616 iaccum = 0;
	
	unsigned char pid_has_been_set = 0;
	unsigned char rx_toggle = 0;
	unsigned char not_asserting_sync = 0;

	initLib();
	setAddr(myaddr = bl_get_addr());
	setMulticast(bl_get_multicast_group(0), 0);
	setMulticast(bl_get_multicast_group(1), 1);
	setMulticast(bl_get_multicast_group(2), 2);
	setMulticast(bl_get_multicast_group(3), 3);
	
	ICR1 = 40000;	//20 ms with 16 Mhz clock with /8 (2Mhz)
	//valid range for pwm is 2000 (1 ms) to 4000 (2 ms)  idle is 3000
	OCR1A = 3000;
	TCCR1A = _BV(COM1A1) | _BV(WGM11);	//clear on match, reset on rollover
	TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS11);		//mode 14 (reset on icr1)   clock div 8
	//TIMSK1 = _BV(TOIE1);
	
	PORTB &= ~(_BV(PB4));	//out 0 on MISO
	DDRB |= _BV(PB4);		//out 0 on MISO

	DDRB |= _BV(PB1);	//enable pwm port

	DDRD &= ~(_BV(PD2) | _BV(PD3));	//enable encoder input
	//encoder 0		a = pd2 b = pd3
	EICRA = _BV(ISC10) | _BV(ISC00);	//both edges on int0 and int1
	EIMSK = _BV(INT0) | _BV(INT1);	//enable int0 and int1
	
	status_dgram = connectDGram(0xF0, 0, 7);	//pc port 7 is incoming status for everything
	gains_dgram = listenDGram(1);
	setpoints_dgram = listenDGram(2);
	hack_sync_dgram = listenDGram(3);
	jumbo_dgram = listenDGram(6);
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
					
					pid_has_been_set = 1;
				}
			}
			
			if(recvDGram(setpoints_dgram, &(packet_buf[0]), &packet_len) == 0)
			{
				if(packet_len == sizeof(setpoints_packet))
				{
					new_setpoint = setpoints->speed;
					waiting_sync = 1;
					//allow pin to float high
					DDRB &= ~(_BV(PB4));
					
					not_asserting_sync = 1;
					rx_toggle ^= 1;
				}
			}
			
			if(recvDGram(jumbo_dgram, &(packet_buf[0]), &packet_len) == 0)
			{
				if(packet_len > 0)
				{
					if(recvJumboDGram(&(packet_buf[0]), packet_len, 0, myaddr, &packet_subset, &packet_subset_len) == 0)
					{
						if(packet_subset_len == sizeof(pid_gains_packet))
						{
							gains2 = packet_subset;
							kp = gains2->p;
							ki = gains2->i;
							kd = gains2->d;
							orientation = gains2->orientation;
							
							pid_has_been_set = 1;
						}
					}
					if(recvJumboDGram(&(packet_buf[0]), packet_len, 1, myaddr, &packet_subset, &packet_subset_len) == 0)
					{
						if(packet_subset_len == sizeof(setpoints_packet))
						{
							setpoints2 = packet_subset;
							new_setpoint = setpoints2->speed;
							waiting_sync = 1;
							//allow pin to float high
							DDRB &= ~(_BV(PB4));
							
							not_asserting_sync = 1;
							rx_toggle ^= 1;
						}
					}
				}
			}
			
			if(recvDGram(hack_sync_dgram, &(packet_buf[0]), &packet_len) == 0)
			{
				if(packet_len == 4)
				{
					if(packet_buf[0] == 0xA5 &&
						packet_buf[1] == 0x5A &&
						packet_buf[2] == 0xCC &&
						packet_buf[3] == 0x33)
					{
						DDRB |= _BV(PB4);		//out 0 on MISO
						
						not_asserting_sync = 0;
					}
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
					
					else if(packet_buf[0] == 0x55 && 
						packet_buf[1] == 0xAA && 
						packet_buf[2] == 0x4D && 
						packet_buf[3] == 0x55)
					{
						bl_program_multicast_groups(packet_buf[4], packet_buf[5], packet_buf[6], packet_buf[7]);
						bl_reboot();
					}
				}
			}
			
			//We are here every 20 ms. We want to run the control loop every 200 ms.
			if(++wait_cnt == 10)
			{
				wait_cnt = 0;
				
				signed int numticks;
				ATOMIC_BLOCK(ATOMIC_FORCEON)
				{
					numticks = position - oldposition;
					oldposition = position;
				}
				
				unsigned char sync_pin_state = PINB & _BV(PB4);
				
				if(waiting_sync)
				{
					if(sync_pin_state || 0 /*debug*/)
					{
						//the pin actually became high
						setpoint = new_setpoint;
						waiting_sync = 0;
					}
				}
				
				//numticks = ticks/200 ms (a velocity measurement)
				
				FIXED1616 e = int_to_fixed(setpoint - numticks);
				
				FIXED1616 d = int_to_fixed(oldnumticks - numticks);		//-d(numticks)/dt	derivative on measurement
				oldnumticks = numticks;
				
				FIXED1616 i = fixed_mult(ki, e);
				iaccum += i;	//Brett Beuregard's trick
				//integrate ki*e instead of integrating e and multiplying by ki
				
				if(iaccum > int_to_fixed(1000))
					iaccum = int_to_fixed(1000);
				else if(iaccum < int_to_fixed(-1000))
					iaccum = int_to_fixed(-1000);
				
				FIXED1616 pout, dout, outf;
				pout = fixed_mult(kp, e);
				dout = fixed_mult(kd, d);
				outf = pout + iaccum + dout;
				
				signed int newval = fixed_to_int(outf);
				if(newval > 1000)
					newval = 1000;
				else if(newval < -1000)
					newval = -1000;
				
				newval += 3000;

				OCR1A = newval;
				
				status->interval_count = interval++;
				status->speed = numticks;
				status->debug_p = pout;
				status->debug_i = iaccum;
				status->debug_d = dout;
				status->out = newval;
				status->time = TCNT1;
				status->debug_bits = (waiting_sync & 1) | ((!!sync_pin_state) << 1) | ((pid_has_been_set & 1 ) << 2) | ((rx_toggle & 1) << 3) | ((not_asserting_sync & 1) << 4);
				
				sendDGram(status_dgram, &(packet_buf[0]), sizeof(wheel_status_packet));
			}
		}
	}
}
