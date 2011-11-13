#ifndef PACKETDATAGRAM_H
#define PACKETDATAGRAM_H

#define DATAGRAM_MAX_SIZE		(MAX_PACKET_SIZE-4)
#define DATAGRAM_PROTOCOL		0x00
#define DATAGRAM_PROTOCOL_MASK	0xC0

extern unsigned char open_dgram_ports[];

extern void *connectDGram(unsigned char addr, unsigned char localport, unsigned char remoteport);
extern void *listenDGram(unsigned char localport);
extern unsigned char sendDGram(void *conn, const unsigned char *packet, unsigned char len);
extern unsigned char recvDGram(void *conn, unsigned char *packet, unsigned char *len);
extern unsigned char recvDGramLL(void *conn, unsigned char *packet, unsigned char *len);
extern void closeDGram(void *conn);

#endif
