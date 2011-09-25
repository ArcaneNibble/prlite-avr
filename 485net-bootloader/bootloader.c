#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/boot.h>
#include <avr/wdt.h>

#define BOOT_EE_START		((void*)(0x3F0))
#define LIBRARY_START_BYTE	((void*)(0x6000))
#define LIBRARY_END_BYTE	((void*)(0x77FF))

typedef struct
{
	unsigned char address;
	unsigned long library_adler;
	unsigned long application_adler;
	unsigned char data[6];
	unsigned char checksum;
} __attribute__((__packed__)) bootdata;

unsigned long adler_a, adler_b;

volatile unsigned char buf[300];
volatile unsigned char bufaddr = 0;
volatile unsigned char wantrx = 0;
volatile unsigned char shouldtx = 1;
volatile unsigned char boot_from_nothing_break = 0;

volatile bootdata b_data;

#define MOD_ADLER 65521

inline void adler_init(void)
{
	adler_a = 1;
	adler_b = 0;
}

void adler_add_byte(unsigned char b)
{
	//adler_a = (adler_a + b) % MOD_ADLER;
	adler_a = (adler_a + b);
	while(adler_a > MOD_ADLER) adler_a -= MOD_ADLER;
	//adler_b = (adler_b + adler_a) % MOD_ADLER;
	adler_b = (adler_b + adler_a);
	while(adler_b > MOD_ADLER) adler_b -= MOD_ADLER;
}

inline unsigned long adler_get_result(void)
{
	return (adler_b << 16) | adler_a;
}

inline void rx_on(void)
{
	PORTC &= ~_BV(PC1);
}

inline void rx_off(void)
{
	PORTC |= _BV(PC1);
}

inline void tx_on(void)
{
	PORTC |= _BV(PC0);
}

inline void tx_off(void)
{
	PORTC &= ~_BV(PC0);
}

inline void timer2_reset(void)
{
	TCNT2 = ~19;		//19 counts to overflow (approx 300 uS)
}

inline void timer2_on(void)
{
	timer2_reset();
	TCCR2B |= _BV(CS22) | _BV(CS21);	//clock div 256 (62.5 kHz)
}

inline void timer2_off(void)
{
	TCCR2B &= ~(_BV(CS22) | _BV(CS21));	//clock div 256 (62.5 kHz)
}

inline void timer2_half_on(void)
{
	TIMSK2 |= _BV(OCIE2A);
}

inline void timer2_half_off(void)
{
	TIMSK2 &= ~(_BV(OCIE2A));
}

inline void timer0_on(void)
{
	TCNT0 = 0;
	TCCR0B |= _BV(CS02) | _BV(CS00);	//clock div 1024 (15.625 kHz)
}

inline void timer0_off(void)
{
	TCCR0B &= ~(_BV(CS00) | _BV(CS00));	//clock div 1024 (15.625 kHz)
}

inline void uart_rx_int_off(void)
{
	UCSR0B &= ~(_BV(RXCIE0));	//enable tx, enable rx, enable rx interrupt
}

inline void uart_rx_int_on(void)
{
	UCSR0B |= _BV(RXCIE0);	//enable tx, enable rx, enable rx interrupt
}

ISR(USART_RX_vect)
{
	//we recieved data
	unsigned char c;
	timer2_reset();
	c = UDR0;
	if(wantrx)
	{
		buf[bufaddr++] = c;
	}
}

ISR(TIMER0_OVF_vect)
{
	static unsigned char cnt = 0;
	
	cnt++;
	
	if(cnt == 20)
	{
		cnt = 0;
		timer0_off();
		shouldtx = 1;
	}
}

ISR(TIMER2_COMPA_vect)
{
	unsigned char b = 0;
	//should hopefully be one packet only
	if(bufaddr == 5 && buf[1] == 0xFE && buf[2] == 0xC0)
	{
		b = buf[0] + buf[1] + buf[2] + buf[3] + buf[4];
		if(b == 0)
		{
			wantrx = 0;
			timer2_off();
			timer0_off();
			
			b_data.address = buf[3];
			
			boot_from_nothing_break = 1;
		}
	}
	bufaddr = 0;
}

ISR(TIMER2_OVF_vect)
{
	unsigned char c;
	//more than 300uS, we can transmit?
	
	if(shouldtx)
	{
		timer0_on();	//retry every second
	
		tx_on();
		uart_rx_int_off();

		while(UCSR0A & _BV(RXC0))
		{
			c = UDR0;
		}

		UDR0 = 0xFE;
		while(!(UCSR0A & _BV(UDRE0)));
		while(!(UCSR0A & _BV(RXC0)));
		c = UDR0;
		if(c != 0xFE)
		{
			uart_rx_int_on();
			tx_off();
			timer2_on();
		}
		
		UDR0 = 0x00;
		while(!(UCSR0A & _BV(UDRE0)));
		while(!(UCSR0A & _BV(RXC0)));
		c = UDR0;
		if(c != 0x00)
		{
			uart_rx_int_on();
			tx_off();
			timer2_on();
		}
		
		UDR0 = 0xC0;
		while(!(UCSR0A & _BV(UDRE0)));
		while(!(UCSR0A & _BV(RXC0)));
		c = UDR0;
		if(c != 0xC0)
		{
			uart_rx_int_on();
			tx_off();
			timer2_on();
		}
		
		UDR0 = 0x42;
		while(!(UCSR0A & _BV(UDRE0)));
		while(!(UCSR0A & _BV(RXC0)));
		c = UDR0;
		if(c != 0x42)
		{
			uart_rx_int_on();
			tx_off();
			timer2_on();
		}
		
		uart_rx_int_on();
		tx_off();
		wantrx = 1;
		
		shouldtx = 0;
	}
}

void flash_info_ee(void)
{
	int i;
	unsigned char a;
	unsigned char *b_data_bytes;
	
	b_data_bytes = &b_data;
	a = 0;
	for(i=0;i<sizeof(b_data)-1;i++)
		a += b_data_bytes[i];
	b_data.checksum = a;
		
	eeprom_write_block(&b_data, BOOT_EE_START, sizeof(b_data));
}

void reset(void)
{
	//WDTCSR = _BV(WDE);
	wdt_enable(WDTO_15MS);
	while(1)
		;
}

void boot_from_nothing(void)
{
	timer2_on();
	timer2_half_on();
	sei();
	
	while(!boot_from_nothing_break)
		;
	
	flash_info_ee();
	reset();
}

inline unsigned long compute_library_checksum(void)
{
}

int main(void)
{
	int i;
	unsigned char a;
	unsigned int w;
	unsigned char *b_data_bytes;
	PGM_VOID_P library;
	
	wdt_reset();
	MCUSR = 0;
	//WDTCSR = 0;		//watchdog off
	wdt_disable();
	
	MCUCR = _BV(IVCE);
	MCUCR = _BV(IVSEL);
	
	

	UBRR0 = 1;				//1 mbaud
	UCSR0A = _BV(U2X0);		//double speed
	UCSR0C = _BV(UCSZ00) | _BV(UCSZ01);	//async uart, 8n1
	UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);	//enable tx, enable rx, enable rx interrupt
	
	TCCR2A = 0;		//no output stuff
	TCCR2B = 0;		//don't enable yet
	OCR2A = ~10;		//160 us
	TIMSK2 = _BV(TOIE2);	//enable overflow interrupt
	
	TCCR0A = 0;		//no output stuff
	TCCR0B = 0;		//don't enable yet
	TIMSK0 = _BV(TOIE0);	//enable overflow interrupt

	DDRC = _BV(PC0) | _BV(PC1);		//output on txe/rxe
	//PORTC = _BV(PC0);	//on tx and rx
	rx_on();
	
	eeprom_read_block(&b_data, BOOT_EE_START, sizeof(b_data));
	b_data_bytes = &b_data;
	a = 0;
	for(i=0;i<sizeof(b_data)-1;i++)
		a += b_data_bytes[i];
	if(a != b_data.checksum)
		boot_from_nothing();
	
	adler_init();
	for(library = LIBRARY_START_BYTE; library <= LIBRARY_END_BYTE; library+=2)
	{
		w = pgm_read_word(library);
		adler_add_byte(w & 0xFF);
		adler_add_byte((w >> 8) & 0xFF);
	}
	if(adler_get_result() != b_data.library_adler)
	{
		tx_on();

		while(1)
		{
			UDR0 = 0x56;
		}
	}
	
	tx_on();

	while(1)
	{
		UDR0 = 0x55;
	}
}
