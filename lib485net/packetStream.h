#ifndef PACKETSTREAM_H
#define PACKETSTREAM_H

#define STREAM_MAX_SIZE			(MAX_PACKET_SIZE-7)
#define STREAM_PROTOCOL			0x40
#define STREAM_PROTOCOL_MASK	0xC0

#define STREAM_TYPE_OPEN_CONN	'S'
#define STREAM_TYPE_CLOSE_CONN	'F'
#define STREAM_TYPE_PAYLOAD		'P'
#define STREAM_TYPE_ACK			'A'
#define STREAM_TYPE_NACK		'N'

#define NACK_CHECKSUM		'C'
#define NACK_NOTLISTENING	'L'
#define NACK_WRONGSEQ		'S'

typedef struct
{
	//0 = unused/closed
	//1 = awaiting ack for open
	//2 = open
	//3 = waiting ack
	//4 = listening
	unsigned char mode;
	unsigned char remote_addr;
	unsigned int tx_seq;
	unsigned int rx_seq;
	unsigned char ports;
	unsigned char tx_packet;
	unsigned char rx_packet;
	//in timer2 overflows
	unsigned char noack_time;
} __attribute__((__packed__)) connData;

#define MAX_CONNECTIONS		8
extern connData conn_states[];

extern void *listenStream(unsigned char localport);
extern void *connectStream(unsigned char addr, unsigned char localport, unsigned char remoteport);
extern unsigned char sendStream(void *conn, const unsigned char *packet, unsigned char len);
extern unsigned char recvStream(void *conn, unsigned char *packet, unsigned char *len);
extern void closeStream(void *conn);

#endif
