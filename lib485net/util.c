#include <avr/io.h>
#include "util.h"
#include "common.h"

void getVersion(char *v)
{
	v[0] = '0';	//major
	v[1] = '2';	//minor
	v[2] = '0';	//revision
	v[3] = '0';	//unused
}

void initLib(void)
{
	unsigned char i;

	//FIXME: is this right?
	__ctors_end();
	
	//initialize timer2
	TCCR2A = 0;
	TCNT2 = 0;
	OCR2A = TICKS_150US;
	OCR2B = TICKS_300US;
	TIFR2 = _BV(OCF2A) | _BV(OCF2B) | _BV(TOV2);
	TIMSK2 = _BV(OCIE2A) | _BV(OCIE2B) | _BV(TOIE2);
	TCCR2B = _BV(CS22) | _BV(CS20);	//clock div 128 (125 kHz)
	
	//initialize uart
	UBRR0 = 1;				//1 mbaud
	UCSR0A = _BV(U2X0);		//double speed
	UCSR0C = _BV(UCSZ00) | _BV(UCSZ01);	//async uart, 8n1
	UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);	//enable tx, enable rx, enable rx interrupt
	
	for(i = 0; i < QUEUE_SIZE; i++)
		tx_queue[i] = 0xff;
		
	for(i = 0; i < QUEUE_SIZE; i++)
		rx_queue[i] = 0xff;
	
	txrx_enable();
	rx_on();
}

void setAddr(unsigned char a)
{
	my_addr = a;
}

unsigned char doChecksum(const unsigned char *buf, unsigned char len)
{
	unsigned char sum, i;
	
	sum = 0xff;
	
	for(i = 0; i < len; i++)
		//sum += buf[i];
		sum = crc8_byte(buf[i], sum);
	
	return sum ^ 0xff;
}
