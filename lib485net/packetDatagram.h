#ifndef PACKETDATAGRAM_H
#define PACKETDATAGRAM_H

#define DATAGRAM_MAX_SIZE		(MAX_PACKET_SIZE-4)
#define DATAGRAM_PROTOCOL		0x00
#define DATAGRAM_PROTOCOL_MASK	0xC0

extern void *connectDGram(unsigned char addr, unsigned char localport, unsigned char remoteport);
extern unsigned char sendDGram(void *conn, const unsigned char *packet, unsigned char len);
extern unsigned char recvDGram(void *conn, unsigned char *packet, unsigned char *len);

#endif
