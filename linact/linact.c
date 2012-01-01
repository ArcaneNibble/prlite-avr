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
			
			curval = read_adc(2);
			
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
