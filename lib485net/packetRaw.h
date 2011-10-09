#ifndef PACKETRAW_H
#define PACKETRAW_H

extern unsigned char sendRaw(const unsigned char *packet, unsigned char len);
extern unsigned char recvRaw(unsigned char *packet, unsigned char *len);
extern unsigned char peekPackets(void);

#endif
