#include <avr/io.h>
#include <avr/interrupt.h>
#include "lib485net.h"
#include "bl_support.h"

typedef struct
{
	unsigned int minval;
	unsigned int maxval;
} __attribute__((__packed__)) linact_setpoints_packet;

typedef struct
{
	unsigned long interval_count;
	unsigned int pos;
	unsigned char arr;
} __attribute__((__packed__)) linact_status_packet;

unsigned int read_adc(char n)
{
	unsigned char tmp0, tmp1;
	ADMUX = _BV(REFS0) | n;	//use avcc, enable channel n
	ADCSRA |= _BV(ADSC);	//start conversion
	
	while(ADCSRA & _BV(ADSC));	//wait for finish
	
	tmp0 = ADCL;
	tmp1 = ADCH;
	
	return ((unsigned int)tmp0) | (((unsigned int)tmp1) << 8);
}

void fw_rev(char fwd)
{
	unsigned char blah = fwd ? _BV(PD6) : _BV(PD5);
	PORTD = (PORTD & (~(_BV(PD6) | _BV(PD5)))) | blah;
	PORTB &= ~(_BV(PB1));
}

void stop(void)
{
	PORTD |= _BV(PD6) | _BV(PD5);
	PORTB &= ~(_BV(PB1));
}

int main(void)
{
	unsigned int minval = 0, maxval = 0x3FF, curval;
	void *setpoints_dgram, *status_dgram;
	unsigned char packet_buf[64];
	unsigned char packet_len;
	linact_setpoints_packet *setpoints;
	linact_status_packet *status;
	unsigned long interval = 0;

	initLib();
	setAddr(bl_get_addr());

	ICR1 = 25000;	//100 ms with 16 Mhz clock with /64 (250 kHz)
	TCCR1A = _BV(WGM11);	//clear on match, reset on rollover
	TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS11) | _BV(CS10);		//mode 14 (reset on icr1)   clock div 64
	
	ADCSRA = _BV(ADEN) | _BV(ADIF) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);	//enable adc, clear any interrupt, div = 128 -> 125 khz
	ADMUX = _BV(REFS0);	//use avcc
	DIDR0 = _BV(ADC0D) | _BV(ADC1D);	//disable digital
	
	DDRD |= _BV(PD5) | _BV(PD6);	//activate the h bridge control pins as outputs
	DDRB |= _BV(PB1);
	
	PORTD &= ~(_BV(PD5) | _BV(PD6));	//and just in case turn them all off
	PORTB &= ~(_BV(PB1));
	
	status_dgram = connectDGram(0xF0, 0, 7);	//pc port 7 is incoming status for everything
	setpoints_dgram = listenDGram(1);
	
	setpoints = status = &(packet_buf[0]);
	
	sei();	//don't think any are enabled here, but library needs it
	
	while(1)
	{
		if(TIFR1 & _BV(TOV1))
		{
			TIFR1 = _BV(TOV1);
			
			if(recvDGram(setpoints_dgram, &(packet_buf[0]), &packet_len) == 0)
			{
				if(packet_len == sizeof(linact_setpoints_packet))
				{
					minval = setpoints->minval;
					maxval = setpoints->maxval;
				}
			}
			
			curval = read_adc(0);
			
			if(curval < minval)			fw_rev(1);
			else if(curval > maxval)	fw_rev(0);
			else stop();
			
			status->interval_count = interval++;
			status->pos = curval;
			status->arr = (curval >= minval) && (curval <= maxval);
			
			sendDGram(status_dgram, &(packet_buf[0]), sizeof(linact_status_packet));
		}
	}
}

#if 0

#include <avr/io.h>
#include <avr/eeprom.h> 
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/twi.h>

typedef signed long FIXED1616;
typedef unsigned long u32;
typedef signed int s16;
typedef unsigned int u16;
typedef unsigned char u8;

#include "i2c-net-packets.h"

unsigned char I2C_SLAVE_ADDR;
unsigned char I2C_DEST_ADDR;
unsigned char tmp_new_dest;

volatile linact_target new_setpoints;
volatile unsigned char setpoints_changed;

volatile wheel_complaints_packet complaints;

volatile linact_position stat;

volatile unsigned char i2c_is_in_use;

ISR(TWI_vect)
{
	static signed char byte_index;
	unsigned char status = TW_STATUS;	//somebody fails, putting this in switch makes a 16 bit compare
	volatile unsigned char * stat_packet = (volatile unsigned char *)(&stat);
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
				case LINACT_SET_POS:
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
			case LINACT_SET_POS:
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
			sum += (TWDR = LINACT_POS_ANNOUNCE_TYPE);
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

unsigned int read_adc(char n)
{
	unsigned char tmp0, tmp1;
	ADMUX = _BV(REFS0) | n;	//use avcc, enable channel n
	ADCSRA |= _BV(ADSC);	//start conversion
	
	while(ADCSRA & _BV(ADSC));	//wait for finish
	
	tmp0 = ADCL;
	tmp1 = ADCH;
	
	return ((unsigned int)tmp0) | (((unsigned int)tmp1) << 8);
}

void fw_rev_0(char fwd)
{
	unsigned char blah = fwd ? _BV(PD0) : _BV(PD1);
	PORTD = (PORTD & (~(_BV(PD0) | _BV(PD1)))) | blah;
}

void stop_0(void)
{
	PORTD = (PORTD & (~(_BV(PD0) | _BV(PD1))));
}

void fw_rev_1(char fwd)
{
	unsigned char blah = fwd ? _BV(PD6) : _BV(PD7);
	PORTD = (PORTD & (~(_BV(PD6) | _BV(PD7)))) | blah;
}

void stop_1(void)
{
	PORTD = (PORTD & (~(_BV(PD6) | _BV(PD7))));
}

int main(void)
{
	unsigned int s0_min = 0, s0_max = 0x3FF, s1_min = 0, s1_max = 0x3FF;
	
	I2C_SLAVE_ADDR = eeprom_read_byte(0);
	if(I2C_SLAVE_ADDR == 0x00 || I2C_SLAVE_ADDR == 0xFF)
	{
		//BAD!
		while(1);
	}

	ICR1 = 25000;	//100 ms with 16 Mhz clock with /64 (250 kHz)
	TCCR1A = _BV(WGM11);	//clear on match, reset on rollover
	TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS11) | _BV(CS10);		//mode 14 (reset on icr1)   clock div 64
	//the timer overflows every 20 ms, but doesn't trigger an interrupt

	TWBR = 12;	//generates 400 khz clock
	TWSR = 0;	//prescaler = 1
	TWAR = I2C_SLAVE_ADDR | 1;	//respond to general call
	TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE);	//enable i2c
	
	ADCSRA = _BV(ADEN) | _BV(ADIF) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);	//enable adc, clear any interrupt, div = 128 -> 125 khz
	ADMUX = _BV(REFS0);	//use avcc
	DIDR0 = _BV(ADC0D) | _BV(ADC1D);	//disable digital
	
	DDRD |= _BV(PD0) | _BV(PD1) | _BV(PD6) |  _BV(PD7);	//using port d 0,1;6,7 for relays

	sei();
	
	while(1)
	{
		unsigned char i2c_in_use_copy;
		unsigned char waiting_for_i2c;
		
		ATOMIC_BLOCK(ATOMIC_FORCEON)
		{
			if(setpoints_changed)
			{
				setpoints_changed = 0;

				s0_min = new_setpoints.t0_min;
				s0_max = new_setpoints.t0_max;
				s1_min = new_setpoints.t1_min;
				s1_max = new_setpoints.t1_max;
			}
		}

		if(TIFR1 & _BV(TOV1))
		{
			unsigned int adc0, adc1;
		
			TIFR1 = _BV(TOV1);
			
			adc0 = read_adc(0);
			adc1 = read_adc(1);
			
			if(adc0 < s0_min)	fw_rev_0(1);
			else if(adc0 > s0_max)	fw_rev_0(0);
			else stop_0();

			if(adc1 < s1_min)	fw_rev_1(1);
			else if(adc1 > s1_max)	fw_rev_1(0);
			else stop_1();
			
			stat.interval_count++;
			stat.pos0 = adc0;
			stat.pos1 = adc1;
			stat.arr0 = (adc0 >= s0_min) && (adc0 <= s0_max);
			stat.arr1 = (adc1 >= s1_min) && (adc1 <= s1_max);

			waiting_for_i2c = 1;
			while(waiting_for_i2c)
			{
				ATOMIC_BLOCK(ATOMIC_FORCEON)
				{
					if(!(i2c_in_use_copy = i2c_is_in_use) || (TIFR1 & _BV(TOV1)))
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

	}

	return 0;
}

#endif