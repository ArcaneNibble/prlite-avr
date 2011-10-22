#include <avr/interrupt.h>
#include <string.h>
#include "common.h"
#include "queue.h"
#include "util.h"
#include "packetDatagram.h"

unsigned char open_dgram_ports[8];

void *connectDGram(unsigned char addr, unsigned char localport, unsigned char remoteport)
{
	if(localport > 7 || remoteport > 7) return NULL;
	open_dgram_ports[localport] = 1;
	return (void*)(addr | (localport << 8) | (remoteport << 11) | (1 << 14));
	//it isn't really a connection
	//1 << 14 is so target 0 from port 0 to port 0 is not null
}

void *listenDGram(unsigned char localport)
{
	if(localport > 7) return NULL;
	open_dgram_ports[localport] = 1;
	return (void*)(0 | (localport << 8) | (0 << 11) | (1 << 14));
}

void closeDGram(void *conn_)
{
	unsigned int conn;
	if(conn_ == NULL)
		return;
	
	conn = (unsigned int)(conn_);
	
	open_dgram_ports[(conn >> 8) & 7] = 0;
}

extern unsigned char sendDGram(void *conn_, const unsigned char *packet, unsigned char len)
{
	unsigned char slot;
	unsigned int conn;
	
	conn = (unsigned int)(conn_);
	
	if(conn_ == NULL || packet == NULL || len == 0 || len > DATAGRAM_MAX_SIZE)
		return 1;
	
	slot = queue_alloc();
	
	if(slot == -1)
		return 1;
	
	//this should be safe, one byte
	packet_queue_status[slot] = len + 4;
	packet_queue[slot * MAX_PACKET_SIZE + 0] = my_addr;		//source address
	packet_queue[slot * MAX_PACKET_SIZE + 1] = conn & 0xFF;	//dest address
	packet_queue[slot * MAX_PACKET_SIZE + 2] = DATAGRAM_PROTOCOL | (((conn >> 8) & 7) << 3) /* sender port */ | ((conn >> 11) & 7) /* receiver port */;
	
	memcpy(&(packet_queue[slot * MAX_PACKET_SIZE + 3]), packet, len);
	
	packet_queue[slot * MAX_PACKET_SIZE + 3 + len] = doChecksum(&(packet_queue[slot * MAX_PACKET_SIZE + 0]), 3 + len);
	
	cli();
	//if we are interrupted after slot is written and before next is incremented
	//and we are so unlucky that a packet was just sent at that time, we will
	//have problems, hence the cli
	tx_queue[tx_queue_next++] = slot;
	sei();
	
	return 0;
}

extern unsigned char recvDGram(void *conn_, unsigned char *packet, unsigned char *len)
{
	unsigned int conn;
	unsigned char slot, queueidx, i, len_, csum;
	
	conn = (unsigned int)(conn_);

	if(conn_ == NULL || packet == NULL || len == NULL)
		return 1;
		
	slot = queueidx = -1;
	
	cli();
	if(rx_queue_next != 0)
	{
		//slot = rx_queue[0];
		for(i = 0; i < rx_queue_next; i++)
		{
			if(((packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 2] & DATAGRAM_PROTOCOL_MASK) == DATAGRAM_PROTOCOL)
				&& ((packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 2] & 7) == ((conn >> 8) & 7)))	//vomit	//what it actually does is check proto and port
			{
				slot = rx_queue[i];
				queueidx = i;
				break;
			}
		}
		
		if(slot != -1)
		{
			for(i = queueidx; i < rx_queue_next-1; i++)
				rx_queue[i] = rx_queue[i+1];
			rx_queue_next--;
		}
	}
	sei();
	
	if(slot == -1)
	{
		*len = 0;
		//this is a success, no packets
		return 0;
	}
	
	len_ = packet_queue_status[slot];
	
	csum = doChecksum(&(packet_queue[slot * MAX_PACKET_SIZE]), len_ - 1);
	if(csum != packet_queue[slot * MAX_PACKET_SIZE + len_ - 1])
	{
		*len = 0;
		//this is a success, but bad checksum
		//fixme: better way to indicate?
		
		queue_free(slot);
		
		return 0;
	}
	
	memcpy(packet, &(packet_queue[slot * MAX_PACKET_SIZE + 3]), len_ - 4);
	*len = len_ - 4;
	
	queue_free(slot);
	
	return 0;
}