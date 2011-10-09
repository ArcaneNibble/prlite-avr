#include <avr/interrupt.h>
#include "queue.h"
#include "common.h"

unsigned char queue_alloc_isr(void)
{
	unsigned char i, ret=0xff;
	
	for(i = 0; i < QUEUE_SIZE; i++)
	{
		if(packet_queue_status[i] == 0)
		{
			packet_queue_status[i] = 1;
			ret = i;
		}
	}
	
	return ret;
}

unsigned char queue_alloc(void)
{
	unsigned char ret;
	
	cli();
	ret = queue_alloc_isr();
	sei();
	
	return ret;
}

void queue_free(unsigned char idx)
{
	packet_queue_status[idx] = 0;
}
