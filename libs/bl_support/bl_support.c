#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>

#include "bl_support.h"

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