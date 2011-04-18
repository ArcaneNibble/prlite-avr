#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/twi.h>

#define I2C_SLAVE_ADDR 0xFE

volatile unsigned char tx_buf[256];
volatile unsigned char tx_len;
volatile unsigned char rx_len;
volatile unsigned char request_tx;
volatile unsigned char request_reply;

const unsigned char *hex = "0123456789ABCDEF";

volatile unsigned char i2c_is_in_use;

void uart_put_hex(unsigned char c)
{
	while(!(UCSR0A & _BV(UDRE0)));
	UDR0 = hex[(c >> 4)];
	while(!(UCSR0A & _BV(UDRE0)));
	UDR0 = hex[c & 0xF];
}

void uart_put_raw(unsigned char c)
{
	while(!(UCSR0A & _BV(UDRE0)));
	UDR0 = c;
}

unsigned char nibble_to_bin(unsigned char a)
{
	if(a >= '0' && a <= '9')
		return a - '0';
	if(a >= 'A' && a <= 'F')
		return a - 'A' + 10;
	if(a >= 'a' && a <= 'f')
		return a - 'a' + 10;
	return 0;
}

unsigned char hex_to_bin(unsigned char a, unsigned char b)
{
	return (nibble_to_bin(a) << 4) | nibble_to_bin(b);
}

ISR(USART_RX_vect)
{
	static unsigned char first_byte = 1;
	static unsigned char wait_for_rx_len = 1;
	static unsigned char tmp_byte;

	if(first_byte)
	{
		tmp_byte = UDR0;
		if(tmp_byte == '.')
			request_reply = 1;
		else if(tmp_byte == ' ')	//avoid cr/lf/crlf issues
		{
			request_tx = 1;
			wait_for_rx_len = 1;
		}
		else
			first_byte = 0;
	}
	else
	{
		first_byte = 1;
		if(wait_for_rx_len)
		{
			wait_for_rx_len = 0;
			rx_len = hex_to_bin(tmp_byte, UDR0);
		}
		else
			tx_buf[tx_len++] = hex_to_bin(tmp_byte, UDR0);
	}
}

ISR(TWI_vect)
{
	unsigned char status = TW_STATUS;	//somebody fails, putting this in switch makes a 16 bit compare
	static unsigned char byte_index;

	switch(status)
	{
	case TW_BUS_ERROR:	//somebody f*cked up
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWSTO);
		break;

	//////////////////////////////////////////////////

	case TW_SR_SLA_ACK:
	case TW_SR_ARB_LOST_SLA_ACK:
	case TW_SR_GCALL_ACK:
	case TW_SR_ARB_LOST_GCALL_ACK:
		i2c_is_in_use = 1;
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
		break;

	case TW_SR_DATA_ACK:
	case TW_SR_GCALL_DATA_ACK:
	case TW_SR_DATA_NACK:
	case TW_SR_GCALL_DATA_NACK:
		uart_put_hex(TWDR);
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
		break;

	case TW_SR_STOP:
		uart_put_raw('\r');
		uart_put_raw('\n');
		i2c_is_in_use = 0;
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
		break;

	//////////////////////////////////////////////////////////////////////////

	case TW_START:
		byte_index = 0;
		i2c_is_in_use = 1;

	case TW_MT_SLA_ACK:
	case TW_MT_SLA_NACK:
	case TW_MT_DATA_ACK:
	case TW_MT_DATA_NACK:
		//send a byte or send stop
		if(byte_index == tx_len)
		{
			if(request_reply)
			{
				TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWSTA);
				request_reply = 0;
			}
			else
			{
				TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWSTO);
				i2c_is_in_use = 0;
			}
			tx_len = 0;
		}
		else
		{
			TWDR = tx_buf[byte_index++];
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
		}
		break;

	case TW_MT_ARB_LOST:
		i2c_is_in_use = 0;
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
		break;

	///////////////////////////////////////////////////////////////

	case TW_REP_START:
		i2c_is_in_use = 1;
		TWDR = tx_buf[0] | TW_READ;	//hack
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
		uart_put_raw('s');
		byte_index = 0;
		break;

	case TW_MR_DATA_ACK:
		uart_put_hex(TWDR);
		byte_index++;

	case TW_MR_SLA_ACK:
		if(byte_index == (rx_len-1))
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
		else
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
		break;

	case TW_MR_SLA_NACK:
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWSTO);
		i2c_is_in_use = 0;
		uart_put_raw('\r');
		uart_put_raw('\n');
		break;

	case TW_MR_DATA_NACK:
		uart_put_hex(TWDR);
		uart_put_raw('\r');
		uart_put_raw('\n');
		byte_index++;
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWSTO);
		i2c_is_in_use = 0;
		break;

	///////////////////////////////////////////////////////////////

	case TW_NO_INFO:
	default:
		//how the hell did i get here?
		break;
	}
}

int main(void)
{
	TWBR = 12;	//generates 400 khz clock
	TWSR = 0;	//prescaler = 1
	TWAR = I2C_SLAVE_ADDR | 1;	//respond to general call
	TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE);	//enable i2c

	UBRR0 = 1;
	UCSR0A = _BV(U2X0);
	UCSR0C = _BV(UCSZ00) | _BV(UCSZ01);
	UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);

	DDRD = _BV(PD1);

	sei();

	while(1)
	{
//		UDR0 = 0x55;
		if(request_tx && (!i2c_is_in_use))
		{
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWSTA);
			request_tx = 0;
		}
	}

	return 0;
}
