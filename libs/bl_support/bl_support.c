#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>

#include "bl_support.h"

typedef struct
{
	unsigned char address;
	unsigned long library_adler;
	unsigned long application_adler;
	unsigned char multicast_group[4];
	unsigned char data[2];
	unsigned char checksum;
} __attribute__((__packed__)) bootdata;

void bl_reboot(void)
{
	wdt_enable(WDTO_15MS);
	while(1)
		;
}

unsigned char bl_get_addr(void)
{
	//checksum should be checked by bootloader
	return eeprom_read_byte((void*)(0x3F0));
}

void bl_erase_lib_csum(void)
{
	bootdata d;
	unsigned char i, a;
	unsigned char *dp;
	
	eeprom_read_block(&d, (void*)(0x3F0), sizeof(d));
	d.library_adler = 0xffffffff;
	
	dp = (unsigned char *)(&d);
	a = 0;
	for(i=0;i<sizeof(d)-1;i++)
		a += dp[i];
	d.checksum = a;
		
	eeprom_write_block(&d, (void*)(0x3F0), sizeof(d));
}

void bl_erase_app_csum(void)
{
	bootdata d;
	unsigned char i, a;
	unsigned char *dp;
	
	eeprom_read_block(&d, (void*)(0x3F0), sizeof(d));
	d.application_adler = 0xffffffff;
	
	dp = (unsigned char *)(&d);
	a = 0;
	for(i=0;i<sizeof(d)-1;i++)
		a += dp[i];
	d.checksum = a;
		
	eeprom_write_block(&d, (void*)(0x3F0), sizeof(d));
}

void bl_erase_all_csum(void)
{
	bootdata d;
	unsigned char i;
	unsigned char *dp;
	
	/*eeprom_read_block(&d, (void*)(0x3F0), sizeof(d));
	d.library_adler = 0xffffffff;
	d.application_adler = 0xffffffff;
	
	dp = (unsigned char *)(&d);
	a = 0;
	for(i=0;i<sizeof(d)-1;i++)
		a += dp[i];
	d.checksum = a;*/
	
	dp = (unsigned char *)(&d);
	for(i=0;i<sizeof(d);i++)
		dp[i] = 0xFF;
		
	eeprom_write_block(&d, (void*)(0x3F0), sizeof(d));
}

unsigned char bl_get_multicast_group(unsigned char which)
{
	if(which > 3)
		return 0xFF;
	return eeprom_read_byte((void*)(0x3F0+9+which));
}

void bl_program_multicast_groups(unsigned char _a, unsigned char _b, unsigned char _c, unsigned char _d)
{
	bootdata d;
	unsigned char i, a;
	unsigned char *dp;
	
	eeprom_read_block(&d, (void*)(0x3F0), sizeof(d));
	d.multicast_group[0] = _a;
	d.multicast_group[1] = _b;
	d.multicast_group[2] = _c;
	d.multicast_group[3] = _d;
	
	dp = (unsigned char *)(&d);
	a = 0;
	for(i=0;i<sizeof(d)-1;i++)
		a += dp[i];
	d.checksum = a;
		
	eeprom_write_block(&d, (void*)(0x3F0), sizeof(d));
}
