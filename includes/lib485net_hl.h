#ifndef LIB485NET_HL_H
#define LIB485NET_HL_H

typedef struct
{
	unsigned char next;
	unsigned char addr;
	unsigned char type;
	unsigned char data[];
} __attribute__((__packed__)) jumbo_dgram_entry;

extern unsigned char recvJumboDGram(unsigned char *packetin, unsigned char lenin, unsigned char type, unsigned char addr, unsigned char **packetout, unsigned char *lenout);

#endif
