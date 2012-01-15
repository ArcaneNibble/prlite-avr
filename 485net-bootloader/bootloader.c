#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/boot.h>
#include <avr/wdt.h>

#define BOOT_EE_START		((void*)(0x3F0))
#define BL_BLOCK_SIZE		32		//dictated by packet
#define BL_BLK_PER_FL_BLK	(SPM_PAGESIZE / BL_BLOCK_SIZE)
#define LIBRARY_START_BYTE	((void*)(0x5000))
#define LIBRARY_END_BYTE	((void*)(0x6FFF))
#define LIBRARY_BLOCKS		((LIBRARY_END_BYTE-LIBRARY_START_BYTE+1) / BL_BLOCK_SIZE)
#define APP_BLOCKS			((int)LIBRARY_START_BYTE / BL_BLOCK_SIZE)

extern const unsigned char crc8_tab[] PROGMEM;

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
volatile unsigned char wantrx;
volatile unsigned char shouldtx;
volatile unsigned char boot_break;

// 0	- address
// 1	- library
// 2	- application
volatile unsigned char booting_mode;
volatile unsigned int booting_block;
volatile unsigned char output_csum;

volatile bootdata b_data;

#define MOD_ADLER 65521

inline void adler_init(void)
{
	adler_a = 1;
	adler_b = 0;
}

inline void adler_add_byte(unsigned char b)
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

#define CRC8_INIT 0xFF
#define CRC8_GOOD 0x9F
unsigned char crc8_byte(unsigned char b, unsigned char crc)
{
	crc = pgm_read_byte(&(crc8_tab[(crc ^ b) & 0xff]));
	return crc;
}

void txrx_enable(void)
{
	DDRC |= _BV(PC0) | _BV(PC1);		//output on txe/rxe
}

void rx_on(void)
{
	PORTC &= ~_BV(PC1);
}

void rx_off(void)
{
	PORTC |= _BV(PC1);
}

void tx_on(void)
{
	PORTC |= _BV(PC0);
}

void tx_off(void)
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
	unsigned char i;
	unsigned char booting_mode_copy;
	
	booting_mode_copy = booting_mode;
	
	//should hopefully be one packet only
	if(booting_mode_copy == 0)
	{
		if(bufaddr == 5 && buf[1] == 0xFE && buf[2] == 0xC0)
		{
			//b = buf[0] + buf[1] + buf[2] + buf[3] + buf[4];
			b = crc8_byte(buf[0], CRC8_INIT);
			b = crc8_byte(buf[1], b);
			b = crc8_byte(buf[2], b);
			b = crc8_byte(buf[3], b);
			b = crc8_byte(buf[4], b);
			if(b == CRC8_GOOD)
			{
				wantrx = 0;
				timer2_off();
				timer0_off();
				
				b_data.address = buf[3];
				
				boot_break = 1;
			}
		}
	}
	else if(booting_mode_copy == 1)
	{
		if(bufaddr == (5 + BL_BLOCK_SIZE) && buf[1] == b_data.address && buf[2] == 0xC1 && buf[3] == (booting_block & 0xFF))
		{
			b = 0xff;
			for(i = 0; i < bufaddr; i++)
				//b += buf[i];
				b = crc8_byte(buf[i], b);
			if(b == CRC8_GOOD)
			{
				wantrx = 0;
				timer2_off();
				timer0_off();
				
				boot_break = 1;
			}
		}
	}
	else if(booting_mode_copy == 2)
	{
		if(bufaddr == (6 + BL_BLOCK_SIZE) && buf[1] == b_data.address && buf[2] == 0xC2 && buf[3] == (booting_block & 0xFF) && buf[4] == ((booting_block >> 8) & 0xFF))
		{
			b = 0xff;
			for(i = 0; i < bufaddr; i++)
				//b += buf[i];
				b = crc8_byte(buf[i], b);
			if(b == CRC8_GOOD)
			{
				wantrx = 0;
				timer2_off();
				timer0_off();
				
				boot_break = 1;
			}
		}
	}
	
	bufaddr = 0;
}

unsigned char tx_with_checking(unsigned char x)
{
	unsigned char c;

	UDR0 = x;
	while(!(UCSR0A & _BV(UDRE0)));
	while(!(UCSR0A & _BV(RXC0)));
	c = UDR0;
	if(c != x)
	{
		uart_rx_int_on();
		tx_off();
		timer2_on();
		return 0;
	}
	return 1;
}

ISR(TIMER2_OVF_vect)
{
	unsigned char c;
	unsigned char booting_mode_copy;
	
	booting_mode_copy = booting_mode;
	
	//more than 300uS, we can transmit?
	
	if(shouldtx)
	{
		timer0_on();	//retry every second
	
		if(booting_mode_copy == 0)
		{
			tx_on();
			uart_rx_int_off();

			while(UCSR0A & _BV(RXC0))
			{
				c = UDR0;
			}
			
			if(!tx_with_checking(0xFE))
				return;
			if(!tx_with_checking(0x00))
				return;
			if(!tx_with_checking(0xC0))
				return;
			if(!tx_with_checking(0xB9))
				return;
			
			uart_rx_int_on();
			tx_off();
			wantrx = 1;
		}
		else if(booting_mode_copy == 1)
		{
			tx_on();
			uart_rx_int_off();

			while(UCSR0A & _BV(RXC0))
			{
				c = UDR0;
			}
			
			if(!tx_with_checking(b_data.address))
				return;
			if(!tx_with_checking(0x00))
				return;
			if(!tx_with_checking(0xC1))
				return;
			if(!tx_with_checking(booting_block))
				return;
			if(!tx_with_checking(output_csum))
				return;
			
			uart_rx_int_on();
			tx_off();
			wantrx = 1;
		}
		else if(booting_mode_copy == 2)
		{
			tx_on();
			uart_rx_int_off();

			while(UCSR0A & _BV(RXC0))
			{
				c = UDR0;
			}
			
			if(!tx_with_checking(b_data.address))
				return;
			if(!tx_with_checking(0x00))
				return;
			if(!tx_with_checking(0xC2))
				return;
			if(!tx_with_checking(booting_block & 0xFF))
				return;
			if(!tx_with_checking((booting_block >> 8) & 0xFF))
				return;
			if(!tx_with_checking(output_csum))
				return;
			
			uart_rx_int_on();
			tx_off();
			wantrx = 1;
		}
		
		shouldtx = 0;
	}
}

void flash_info_ee(void)
{
	unsigned char i;
	unsigned char a;
	unsigned char *b_data_bytes;
	
	b_data_bytes = (unsigned char *)(&b_data);
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

//inclusive
void compute_adler(void *addr, void *end_addr)
{
	unsigned char a;
	adler_init();
	for(; addr <= end_addr; addr++)
	{
		a = pgm_read_byte(addr);
		adler_add_byte(a);
	}
}

void boot_from_nothing(void)
{
	wantrx = 0;
	boot_break = 0;

	booting_mode = 0;

	shouldtx = 1;
	timer2_on();
	timer2_half_on();
	sei();
	
	while(!boot_break)
		;
	
	flash_info_ee();
	reset();
}

//inclusive [start,end] not [start,end)
void erase_region(void *start, void *end)
{
	void *addr;
	
	for(addr = start; addr <= end; addr += SPM_PAGESIZE)
	{
		boot_page_erase_safe(addr);
	}
}

void boot_common(void *addr, unsigned char cmdbyte, unsigned int blocks, unsigned char hack)
{
	unsigned char i;
	unsigned int w;
	while(booting_block < blocks)
	{
		if(!hack)
		{
			//output_csum = ~(b_data.address + 0x00 + cmdbyte + booting_block) + 1;
			output_csum = 0xFF;
			output_csum = crc8_byte(b_data.address, output_csum);
			output_csum = crc8_byte(0x00, output_csum);
			output_csum = crc8_byte(cmdbyte, output_csum);
			output_csum = crc8_byte(booting_block, output_csum);
			output_csum = output_csum ^ 0xFF;
		}
		else
		{
			//booting app
			//output_csum = ~(b_data.address + 0x00 + cmdbyte + (booting_block & 0xFF) + ((booting_block >> 8) & 0xFF)) + 1;
			output_csum = 0xFF;
			output_csum = crc8_byte(b_data.address, output_csum);
			output_csum = crc8_byte(0x00, output_csum);
			output_csum = crc8_byte(cmdbyte, output_csum);
			output_csum = crc8_byte(booting_block & 0xFF, output_csum);
			output_csum = crc8_byte((booting_block >> 8) & 0xFF, output_csum);
			output_csum = output_csum ^ 0xFF;
		}
	
		wantrx = 0;
		boot_break = 0;
	
		shouldtx = 1;
		timer2_on();
		timer2_half_on();
		sei();
		
		while(!boot_break)
			;
		
		cli();
		
		for(i = 0; i < BL_BLOCK_SIZE; i += 2)
		{
			w = buf[4 + i + hack] | (buf[4 + i + hack + 1] << 8);
			boot_page_fill_safe(addr, w);
			addr += 2;
		}
		
		booting_block++;
		
		if((booting_block % BL_BLK_PER_FL_BLK) == 0)
		{
			//hopefully correct
			boot_page_write_safe(addr - SPM_PAGESIZE);
		}
	}
	
	boot_rww_enable_safe();
}

void boot_load_library(void)
{
	erase_region(LIBRARY_START_BYTE, LIBRARY_END_BYTE);
	
	booting_mode = 1;
	booting_block = 0;
	
	boot_common(LIBRARY_START_BYTE, 0xC1, LIBRARY_BLOCKS, 0);
	
	compute_adler(LIBRARY_START_BYTE, LIBRARY_END_BYTE);
	b_data.library_adler = adler_get_result();
	
	flash_info_ee();
	
	reset();
}

void boot_load_app(void)
{
	erase_region(0, LIBRARY_START_BYTE-1);
	
	booting_mode = 2;
	booting_block = 0;

	boot_common(0, 0xC2, APP_BLOCKS, 1);
	
	compute_adler(0, LIBRARY_START_BYTE - 1);
	b_data.application_adler = adler_get_result();
	
	flash_info_ee();
	
	reset();
}

void bootloader_hw_enable(void)
{
	MCUCR = _BV(IVCE);
	MCUCR = _BV(IVSEL);
	
	///

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

	txrx_enable();
	//PORTC = _BV(PC0);	//on tx and rx
	rx_on();
}

int main(void)
{
	unsigned char i;
	unsigned char a;
	unsigned char *b_data_bytes;
	void (*zero)(void) = 0;
	
	wdt_reset();
	MCUSR &= ~(_BV(WDRF));
	//WDTCSR = 0;		//watchdog off
	wdt_disable();
	
	//////////////////////////////////////////////////////
	
	eeprom_read_block(&b_data, BOOT_EE_START, sizeof(b_data));
	b_data_bytes = (unsigned char *)&b_data;
	a = 0;
	for(i=0;i<sizeof(b_data)-1;i++)
		a += b_data_bytes[i];
	if(a != b_data.checksum)
	{
		bootloader_hw_enable();
		boot_from_nothing();
	}
	
	//////////////////////////////////////////////////////
	
	compute_adler(LIBRARY_START_BYTE, LIBRARY_END_BYTE);
	if(adler_get_result() != b_data.library_adler)
	{
		bootloader_hw_enable();
		boot_load_library();
	}
	
	///////////////////////////////////////////////////////
	
	compute_adler(0, LIBRARY_START_BYTE - 1);
	if(adler_get_result() != b_data.application_adler)
	{
		bootloader_hw_enable();
		boot_load_app();
	}
	
	//goodbye!
	
	//just in case
	boot_rww_enable_safe();
	zero();
}
