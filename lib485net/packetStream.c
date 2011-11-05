#include <avr/interrupt.h>
#include <string.h>
#include "common.h"
#include "queue.h"
#include "util.h"
#include "packetStream.h"
#include "packetRaw.h"

connData conn_states[MAX_CONNECTIONS];

extern void *listenStream(unsigned char localport)
{
	unsigned char i;
	connData *d = NULL;
	
	if(localport > 7) return NULL;
	
	for(i = 0; i < MAX_CONNECTIONS; i++)
	{
		if(conn_states[i].mode == 0)
		{
			d = &(conn_states[i]);
			break;
		}
	}
	if(d == NULL) return NULL;
	d->mode = 4;
	d->remote_addr = 0;
	d->tx_seq = 0;
	d->rx_seq = 0;
	d->ports = localport << 3;
	d->rx_packet = 0xff;
	d->tx_packet = 0xff;
	d->noack_time = 0;
	
	return d;
}

extern void *connectStream(unsigned char addr, unsigned char localport, unsigned char remoteport)
{
	unsigned char i;
	connData *d = NULL;
	unsigned char buf_tmp[7];
	
	buf_tmp[0] = my_addr;
	buf_tmp[1] = addr;
	buf_tmp[2] = (localport << 3) | remoteport | STREAM_PROTOCOL;
	//payload now
	buf_tmp[3] = STREAM_TYPE_OPEN_CONN;
	buf_tmp[4] = buf_tmp[5] = 0;
	buf_tmp[6] = doChecksum(buf_tmp, 6);
	
	if(localport > 7 || remoteport > 7) return NULL;
	
	for(i = 0; i < MAX_CONNECTIONS; i++)
	{
		if(conn_states[i].mode == 0)
		{
			d = &(conn_states[i]);
			break;
		}
	}
	if(d == NULL) return NULL;
	d->mode = 1;
	d->remote_addr = addr;
	d->tx_seq = 0;
	d->rx_seq = 0;
	d->ports = (localport << 3) | remoteport;
	d->rx_packet = 0xff;
	d->tx_packet = 0xff;
	d->noack_time = 0;
	
	sendRaw(buf_tmp, sizeof(buf_tmp));
	
	return d;
}

//0xff means we can retry it later manually (need better)
extern unsigned char sendStream(void *conn, const unsigned char *packet, unsigned char len)
{
	connData *d;
	unsigned char slot;
	
	if(conn == NULL || packet == NULL || len == 0 || len > STREAM_MAX_SIZE)
		return 1;
	d = (connData*)(conn);
	if(d->mode == 0 || d->mode == 100)
		return 1;
	if(d->mode != 2)
		return 0xff;
	
	//so, we should be essentially idle now
	
	slot = queue_alloc();
	
	if(slot == 0xff)
		return 1;
		
	packet_queue_status[slot] = len + 7;
	packet_queue[slot * MAX_PACKET_SIZE + 0] = my_addr;			//source address
	packet_queue[slot * MAX_PACKET_SIZE + 1] = d->remote_addr;	//dest address
	packet_queue[slot * MAX_PACKET_SIZE + 2] = STREAM_PROTOCOL | d->ports;
	packet_queue[slot * MAX_PACKET_SIZE + 3] = STREAM_TYPE_PAYLOAD;	//payload
	packet_queue[slot * MAX_PACKET_SIZE + 4] = d->tx_seq & 0xFF;
	packet_queue[slot * MAX_PACKET_SIZE + 5] = (d->tx_seq >> 8) & 0xFF;
	memcpy(&(packet_queue[slot * MAX_PACKET_SIZE + 6]), packet, len);
	packet_queue[slot * MAX_PACKET_SIZE + 7 + len] = doChecksum(&(packet_queue[slot * MAX_PACKET_SIZE + 0]), 6 + len);
	
	d->tx_packet = slot;
	d->tx_seq++;
	d->mode = 3;
	
	cli();
	tx_queue[tx_queue_next++] = slot | 0x80;
	sei();
	
	return 0;
}

extern unsigned char recvStream(void *conn, unsigned char *packet, unsigned char *len)
{
	unsigned long len_;
	connData *d;
	
	if(conn == NULL || packet == NULL || len == NULL)
		return 1;
	d = (connData*)(conn);
	if(d->mode == 0 || d->mode == 100)
		return 1;
	if(d->mode != 2)
		return 0xff;
		
	if(d->rx_packet == 0xff) return 0xff;
	
	//we have a packet!
	//the idle loop should check checksums, so this is valid
	//the idle loop should also have removed it from the rx queue
	
	len_ = packet_queue_status[d->rx_packet];
	
	memcpy(packet, &(packet_queue[d->rx_packet * MAX_PACKET_SIZE + 6]), len_ - 7);
	*len = len_ - 7;
	
	queue_free(d->rx_packet);
	
	return 0;
}

extern void closeStream(void *conn)
{
	connData *d;
	unsigned char buf_tmp[7];
	
	if(conn == NULL) return;
	
	d = (connData*)(conn);
	
	buf_tmp[0] = my_addr;
	buf_tmp[1] = d->remote_addr;
	buf_tmp[2] = d->ports | STREAM_PROTOCOL;
	//payload now
	buf_tmp[3] = STREAM_TYPE_CLOSE_CONN;
	buf_tmp[4] = buf_tmp[5] = 0;
	buf_tmp[6] = doChecksum(buf_tmp, 6);
	
	//close is valid in any state
	d->mode = 0;
	d->remote_addr = 0;
	d->tx_seq = 0;
	d->rx_seq = 0;
	d->ports = 0;
	d->rx_packet = 0xff;
	d->tx_packet = 0xff;
	d->noack_time = 0;
	
	sendRaw(buf_tmp, sizeof(buf_tmp));
}
