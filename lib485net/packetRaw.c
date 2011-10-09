#include <avr/interrupt.h>
#include <string.h>
#include "common.h"
#include "queue.h"

unsigned char sendRaw(const unsigned char *packet, unsigned char len)
{
	unsigned char slot;
	
	if(packet == NULL || len == 0 || len > MAX_PACKET_SIZE)
		return 1;
	
	slot = queue_alloc();
	
	if(slot == -1)
		return 1;
	
	//this should be safe, one byte
	packet_queue_status[slot] = len;
	memcpy(&(packet_queue[slot * MAX_PACKET_SIZE]), packet, len);
	
	cli();
	//if we are interrupted after slot is written and before next is incremented
	//and we are so unlucky that a packet was just sent at that time, we will
	//have problems, hence the cli
	tx_queue[tx_queue_next++] = slot;
	sei();
	
	return 0;
}

unsigned char recvRaw(unsigned char *packet, unsigned char *len)
{
	unsigned char slot, i, len_;

	if(packet == NULL || len == 0)
		return 1;
	
	cli();
	if(rx_queue_next == 0) slot = -1;
	else
	{
		slot = rx_queue[0];
		
		for(i = 0; i < rx_queue_next-1; i++)
			rx_queue[i] = rx_queue[i+1];
		rx_queue_next--;
	}
	sei();
	
	if(slot == -1)
	{
		*len = 0;
		//this is a success, no packets
		return 0;
	}
	
	len_ = packet_queue_status[slot];
	memcpy(packet, &(packet_queue[slot * MAX_PACKET_SIZE]), len_);
	*len = len_;
	
	queue_free(slot);
	
	return 0;
}

unsigned char peekPackets(void)
{
	return rx_queue_next;
}
