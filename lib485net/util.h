#ifndef UTIL_H
#define UTIL_H

extern void txrx_enable(void);
extern void tx_on(void);
extern void tx_off(void);
extern void rx_on(void);
extern void rx_off(void);
extern unsigned char crc8_byte(unsigned char b, unsigned char crc);

//4 bytes required at v
extern void getVersion(char *v);
//WE USE TIMER2
extern void initLib(void);

//0 = recieve anybody
extern void setAddr(unsigned char a);

extern void setMulticast(unsigned char group, unsigned char which);

extern unsigned char doChecksum(const unsigned char *buf, unsigned char len);

//actually 37.5
#define TICKS_300US		38
//actually 18.75
#define TICKS_150US		19
//24 us
#define TICKS_1BYTETIMEOUT	3

#endif
