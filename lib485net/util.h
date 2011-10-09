#ifndef UTIL_H
#define UTIL_H

static const void __attribute__ ((unused)) (*txrx_enable)(void) = (const void (*)(void))0x7ff8;
static const void __attribute__ ((unused)) (*tx_on)(void) = (const void (*)(void))0x7ff0;
static const void __attribute__ ((unused)) (*tx_off)(void) = (const void (*)(void))0x7ff2;
static const void __attribute__ ((unused)) (*rx_on)(void) = (const void (*)(void))0x7ff4;
static const void __attribute__ ((unused)) (*rx_off)(void) = (const void (*)(void))0x7ff6;

//4 bytes required at v
extern void getVersion(char *v);
//WE USE TIMER2
extern void initLib(void);

//0 = recieve anybody
extern void setAddr(unsigned char a);

extern unsigned char doChecksum(const unsigned char *buf, unsigned char len);

//actually 37.5
#define TICKS_300US		38
//actually 18.75
#define TICKS_150US		19

#endif
