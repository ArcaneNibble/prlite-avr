#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "lib485net.h"
#include "bl_support.h"

typedef signed long FIXED1616;

typedef struct
{
	FIXED1616 p;
	FIXED1616 i;
	FIXED1616 d;
	unsigned char orientation;
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
inline FIXED1616 fixed_mult(FIXED1616 a, FIXED1616 b)
{
	signed long long tmp;

	tmp = (signed long long)(a);
	tmp *= (signed long long)(b);

	tmp >>= 16;

	return (FIXED1616)tmp;
}

#if 0
typedef unsigned long u32;
typedef signed int s16;
typedef unsigned int u16;
typedef unsigned char u8;

#include "i2c-net-packets.h"

unsigned char I2C_SLAVE_ADDR;
unsigned char I2C_DEST_ADDR;
unsigned char tmp_new_dest;

volatile pid_gains_packet new_gains;
volatile unsigned char gains_changed;

volatile setpoints_packet new_setpoints;
volatile unsigned char setpoints_changed;

volatile wheel_complaints_packet complaints;

volatile wheel_status_packet stat;

volatile int ticks0;
volatile int ticks1;

volatile unsigned long interval_count;
volatile int ticks0_interval;
volatile int ticks1_interval;

volatile unsigned char do_some_math;

volatile unsigned char i2c_is_in_use;

ISR(TWI_vect)
{
	static signed char byte_index;
	unsigned char status = TW_STATUS;	//somebody fails, putting this in switch makes a 16 bit compare
	volatile unsigned char * stat_packet = (volatile unsigned char *)(&stat);
	volatile unsigned char * gains_packet = (volatile unsigned char *)(&new_gains);
	volatile unsigned char * new_setpoints_packet = (volatile unsigned char *)(&new_setpoints);
	volatile unsigned char * char_complaints_packet = (volatile unsigned char *)(&complaints);
	static unsigned char sum;
	static unsigned char cur_sync_cmd = ARE_YOU_ALIVE_REQUEST_TYPE;
	static unsigned char sum_ok = 0;
	static unsigned char rx_len;
	static unsigned char tx_len;
	unsigned char byte;

	switch(status)
	{
	case TW_BUS_ERROR:	//somebody f*cked up
		complaints.i2c_bus_error++;
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWSTO);
		break;

	////////////////////////////////////////////////////////////////////

	case TW_ST_SLA_ACK:
	case TW_ST_ARB_LOST_SLA_ACK:
		//slave tx mode
		byte_index = -1;
		i2c_is_in_use = 1;
		sum = (TWDR = sum_ok);
		if(sum_ok && cur_sync_cmd < sizeof(cmd_has_reply) && (tx_len = cmd_has_reply[cur_sync_cmd]))
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
		else
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
		break;
			

	case TW_ST_DATA_ACK:
		//data done transmitting, ack --> next byte
		if(byte_index == -1)
		{
			sum += (TWDR = cmd_reply[cur_sync_cmd]);
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
			byte_index++;
		}
		else if(byte_index == tx_len)
		{
			TWDR = sum;
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
		}
		else
		{
			switch(cur_sync_cmd)
			{
			case ARE_YOU_ALIVE_REQUEST_TYPE:
				sum += (TWDR = 0xAA);
				TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
				break;
			case GET_STATS_REQUEST_TYPE:
				sum += (TWDR = char_complaints_packet[byte_index]);
				TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
				break;
			default:
				//WTF?
				complaints.state_machine_fubar++;
				TWDR = 0xFF;
				TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
				break;
			}
			byte_index++;
		}
		break;

	case TW_ST_DATA_NACK:
	case TW_ST_LAST_DATA:
		//slave tx, master wants us to shut up or we are done or both
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);	//get ready to start again
		i2c_is_in_use = 0;
		cur_sync_cmd = ARE_YOU_ALIVE_REQUEST_TYPE;
		sum_ok = 0;
		break;

	//////////////////////////////////////////////////////////////////////////

	case TW_SR_SLA_ACK:
	case TW_SR_ARB_LOST_SLA_ACK:
	case TW_SR_GCALL_ACK:
	case TW_SR_ARB_LOST_GCALL_ACK:
		byte_index = -2;
		i2c_is_in_use = 1;
		sum = 0;
		sum_ok = 0;
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
		break;

	case TW_SR_DATA_NACK:
	case TW_SR_GCALL_DATA_NACK:
		i2c_is_in_use = 0;

	case TW_SR_DATA_ACK:
	case TW_SR_GCALL_DATA_ACK:
		if(byte_index == -2) //src
		{
			sum += TWDR;
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
			byte_index++;
		}
		else if(byte_index == -1) //type
		{
			sum += (cur_sync_cmd = TWDR);
			byte_index++;
			if(cur_sync_cmd < sizeof(cmd_has_data) && (rx_len = cmd_has_data[cur_sync_cmd]))
				TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
			else
				TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
		}
		else if(byte_index == rx_len)
		{
			if(cur_sync_cmd < sizeof(cmd_has_data))
			{
				sum_ok = (sum == TWDR);
				if(!sum_ok)
					complaints.bad_checksum_packet++;
			}
			else
				sum_ok = 0;	//if we don't do this, we will interpret the byte after the command byte as a checksum, which is bad

			if(sum_ok)
			{
				//we have to put this here so we don't do anything if the checksum is bad
				switch(cur_sync_cmd)
				{
				case SET_PID_GAINS_REQUEST_TYPE:
					gains_changed = 1;
					break;
				case SET_SETPOINTS_REQUEST_TYPE:
					setpoints_changed = 1;
					break;
				case SET_I2C_TARGET_ADDR:
					I2C_DEST_ADDR = tmp_new_dest;
					break;
				default:
					//this always happens on every recieved command, so not a fubar
					break;
				}
			}

			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);	//expecting stop now
		}
		else
		{
			//recieve the byte
			sum += (byte = TWDR);
			switch(cur_sync_cmd)
			{
			case SET_PID_GAINS_REQUEST_TYPE:
				gains_packet[byte_index] = byte;
				break;
			case SET_SETPOINTS_REQUEST_TYPE:
				new_setpoints_packet[byte_index] = byte;
				break;
			case SET_I2C_TARGET_ADDR:
				tmp_new_dest = byte;
				break;
			default:
				//we should not be recieving bytes if we don't recognize the command
				complaints.state_machine_fubar++;
				break;
			}
			if(byte_index != rx_len - 1)
				TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
			else
				TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
			byte_index++;
		}
		break;

	case TW_SR_STOP:
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);	//get ready to start again
		i2c_is_in_use = 0;
		break;

	//////////////////////////////////////////////////////////////////////////

	case TW_START:
		i2c_is_in_use = 1;	//do i need this?
		byte_index = -2;
		sum = 0;
		TWDR = I2C_DEST_ADDR | TW_WRITE;
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
		break;

	case TW_MT_SLA_ACK:
	case TW_MT_SLA_NACK:
	case TW_MT_DATA_ACK:
	case TW_MT_DATA_NACK:
		//send a byte or send stop
		if(byte_index == -2)
		{
			sum += (TWDR = I2C_SLAVE_ADDR);
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
			byte_index++;
		}
		else if(byte_index == -1)
		{
			sum += (TWDR = WHEEL_STATUS_ANNOUNCE_TYPE);
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
			byte_index++;
		}
		else if(byte_index == (sizeof(stat)))
		{
			TWDR = sum;
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
			byte_index++;
		}
		else if(byte_index == (sizeof(stat)+1))
		{
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWSTO);
			i2c_is_in_use = 0;
		}
		else
		{
			sum += (TWDR = stat_packet[byte_index++]);
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
		}
		break;

	case TW_MT_ARB_LOST:
		i2c_is_in_use = 0;	//extra?
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
		complaints.arbitration_dropped++;
		break;

	/////////////////////////////////////////////////////////

	case TW_NO_INFO:
	default:
		//how the hell did i get here?
		complaints.state_machine_fubar++;
		break;
	}
}


ISR(TIMER1_OVF_vect)
{
	static unsigned char times = 0;
	times++;
	if(times == 5)
	{
		times = 0;
		//this should happen every 100 ms
		interval_count++;
		ticks0_interval = ticks0;
		ticks1_interval = ticks1;
		ticks0 = 0;
		ticks1 = 0;
		do_some_math = 1;
	}
}

int main(void)
{
	I2C_SLAVE_ADDR = eeprom_read_byte(0);
	if(I2C_SLAVE_ADDR == 0x00 || I2C_SLAVE_ADDR == 0xFF)
	{
		//BAD!
		while(1);
	}

	ICR1 = 40000;	//20 ms with 16 Mhz clock with /8 (2Mhz)
	//valid range for pwm is 2000 (1 ms) to 4000 (2 ms)  idle is 3000
	OCR1A = 3000;
	OCR1B = 3000;
	TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM11);	//clear on match, reset on rollover
	TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS11);		//mode 14 (reset on icr1)   clock div 8
	TIMSK1 = _BV(TOIE1);

	DDRB |= _BV(PB1) | _BV(PB2);	//enable pwm port

	DDRD &= ~(_BV(PD2) | _BV(PD3) | _BV(PD4) | _BV(PD5));	//enable encoder input
	//encoder 0		a = pd2 b = pd4
	//encoder 1		a = pd3 b = pd5
	EICRA = _BV(ISC10) | _BV(ISC00);	//both edges on int0 and int1
	EIMSK = _BV(INT0) | _BV(INT1);	//enable int0 and int1

	TWBR = 12;	//generates 400 khz clock
	TWSR = 0;	//prescaler = 1
	TWAR = I2C_SLAVE_ADDR | 1;	//respond to general call
	TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE);	//enable i2c

	sei();

	FIXED1616 w0_p = 0;
	FIXED1616 w0_i = 0;
	FIXED1616 w0_d = 0;
	FIXED1616 w1_p = 0;
	FIXED1616 w1_i = 0;
	FIXED1616 w1_d = 0;

	int s0 = 0;
	int s1 = 0;

	FIXED1616 i0 = 0, i1 = 0;
	FIXED1616 last0 = 0, last1 = 0;

	unsigned long last_interval = 0;

	while(1)
	{
		unsigned long interval_count_copy;
		int ticks0_interval_copy;
		int ticks1_interval_copy;

		unsigned char i2c_in_use_copy;

		unsigned char waiting_for_i2c;

the_beginning_of_the_loop:

		ATOMIC_BLOCK(ATOMIC_FORCEON)
		{
			if(gains_changed)
			{
				gains_changed = 0;

				w0_p = new_gains.w0_p;
				w0_i = new_gains.w0_i;
				w0_d = new_gains.w0_d;
				w1_p = new_gains.w1_p;
				w1_i = new_gains.w1_i;
				w1_d = new_gains.w1_d;
			}
			if(setpoints_changed)
			{
				setpoints_changed = 0;

				s0 = new_setpoints.s0;
				s1 = new_setpoints.s1;
			}
			if(!do_some_math) goto the_beginning_of_the_loop;
			do_some_math = 0;
			interval_count_copy = interval_count;
			ticks0_interval_copy = ticks0_interval;
			ticks1_interval_copy = ticks1_interval;
		}

		if(interval_count_copy - last_interval != 1)
			complaints.missed_interval++;
		last_interval = interval_count_copy;

		{
			//assume that only one interval has passed
			int e0, e1;

			e0 = s0 - ticks0_interval_copy;
			e1 = s1 - ticks1_interval_copy;

			FIXED1616 e0f, e1f;

			e0f = int_to_fixed(e0);
			e1f = int_to_fixed(e1);

			i0 += e0f;
			i1 += e1f;

			FIXED1616 d0, d1;
			d0 = e0f - last0;
			d1 = e1f - last1;
			last0 = e0f;
			last1 = e1f;

			FIXED1616 pout0, pout1;
			FIXED1616 iout0, iout1;
			FIXED1616 dout0, dout1;

			pout0 = fixed_mult(w0_p, e0f);
			pout1 = fixed_mult(w1_p, e1f);
			iout0 = fixed_mult(w0_i, i0);
			iout1 = fixed_mult(w1_i, i1);
			dout0 = fixed_mult(w0_d, d0);
			dout1 = fixed_mult(w1_d, d1);

			FIXED1616 out0, out1;
			out0 = pout0 + iout0 + dout0;
			out1 = pout1 + iout1 + dout1;

			int new0, new1;
			new0 = fixed_to_int(out0);
			new1 = fixed_to_int(out1);

			new0 += 3000;
			new1 += 3000;

			if(new0 > 4000) new0 = 4000;
			if(new0 < 2000) new0 = 2000;
			if(new1 > 4000) new1 = 4000;
			if(new1 < 2000) new1 = 2000;

			OCR1A = new0;
			OCR1B = new1;

			stat.debug_p = pout0;
			stat.debug_i = iout0;
			stat.debug_d = dout0;
			stat.out = new0;
			stat.time = TCNT1;
		}

		stat.interval_count = interval_count_copy;
		stat.ticks0_interval = ticks0_interval_copy;
		stat.ticks1_interval = ticks1_interval_copy;

		waiting_for_i2c = 1;
		while(waiting_for_i2c)
		{
			ATOMIC_BLOCK(ATOMIC_FORCEON)
			{
				if(!(i2c_in_use_copy = i2c_is_in_use) || do_some_math)
				{
					waiting_for_i2c=0;

					if(!i2c_in_use_copy)
					{
						TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWSTA);	//start?
					}
					else
					{
						complaints.busy_dropped++;
					}
				}
			}
		}
	}

	return 0;
}
#endif

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
	signed int currentspeed;
	void *gains_dgram, *setpoints_dgram, *status_dgram;
	unsigned long interval = 0;
	signed int oldposition = 0;
	unsigned char packet_buf[64];
	unsigned char packet_len;
	pid_gains_packet *gains;
	setpoints_packet *setpoints;
	wheel_status_packet *status;

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
				}
			}
			
			if(recvDGram(setpoints_dgram, &(packet_buf[0]), &packet_len) == 0)
			{
				if(packet_len == sizeof(setpoints_packet))
				{
					setpoint = setpoints->speed;
				}
			}
			
			
			ATOMIC_BLOCK(ATOMIC_FORCEON)
			{
				currentspeed = position - oldposition;
				oldposition = position;
			}
			
			{
				signed int err;
				FIXED1616 errf;
				static FIXED1616 i = 0;
				static FIXED1616 old_err = 0;
				FIXED1616 d;
				FIXED1616 pout, iout, dout, outf;
				signed int newval;
				
				err = setpoint - currentspeed;
				errf = int_to_fixed(err);
				
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
				status->speed = currentspeed;
				status->debug_p = pout;
				status->debug_i = iout;
				status->debug_d = dout;
				status->out = newval;
				status->time = TCNT1;
			}
			
			sendDGram(status_dgram, &(packet_buf[0]), sizeof(wheel_status_packet));
		}
	}
}
