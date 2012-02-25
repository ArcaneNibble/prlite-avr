#include <avr/io.h>
#include "util.h"
#include "common.h"
#include "queue.h"

void uart_rx_isr(void) __attribute__((signal));
void uart_tx_isr(void) __attribute__((signal));
void t2_150(void) __attribute__((signal));
void t2_300(void) __attribute__((signal));

unsigned char current_rx_queue_slot = 0xFF;
unsigned char rx_packet_bytes;
unsigned char srcaddr_keep;
unsigned char ignore_bytes;

unsigned char current_tx_queue_slot = 0xFF;
unsigned char tx_packet_bytes;
unsigned char tx_packet_bytes_max;

void uart_rx_isr(void)
{
	unsigned char c;
	
	c = UDR0;
	
	if(!ignore_bytes)
	{
		if(srcaddr_keep == 0)
		{
			//no packet right now
			srcaddr_keep = c;
		}
		else
		{
			if(current_rx_queue_slot == 0xFF)
			{
				//2nd byte (dest addr)
				if(c == my_addr || my_addr == 0)
				{
					current_rx_queue_slot = queue_alloc_isr();
					
					if(current_rx_queue_slot != 0xFF)
					{
						//if it didn't fail
						rx_packet_bytes = 2;
						packet_queue[current_rx_queue_slot * MAX_PACKET_SIZE + 0] = srcaddr_keep;
						packet_queue[current_rx_queue_slot * MAX_PACKET_SIZE + 1] = c;
					}
					else
					{
						//we ran out of room!
						rx_overruns++;
						ignore_bytes = 1;
					}
				}
				else
				{
					//not interested
					ignore_bytes = 1;
				}
			}
			else
			{
				//we aren't ignoring stuff, we have a packet, we have a queue slot --> normal
				if(rx_packet_bytes < MAX_PACKET_SIZE)
					packet_queue[current_rx_queue_slot * MAX_PACKET_SIZE + rx_packet_bytes++] = c;
				else
					//somebody made a protocol error
					protocol_errors++;
			}
		}
	}
	
	//delay interrupt
	OCR2A = TCNT2 + TICKS_150US;
	OCR2B = TCNT2 + TICKS_300US + (my_addr & 0x3F) * 2;
	
	//clear pin change flag
	PCIFR = _BV(PCIF2);
}

void t2_150(void)
{
	//this isr is processed when a packet is done being recieved
	if(current_rx_queue_slot != 0xFF)
	{
		//we were receiving a packet that we cared about
		
		//reuse to store size
		packet_queue_status[current_rx_queue_slot] = rx_packet_bytes;
		
		//it isn't possible to overrun this
		rx_queue[rx_queue_next++] = current_rx_queue_slot;
		
		//we don't dequeue the packet yet, the processing logic does that
	}
	
	current_rx_queue_slot = 0xFF;
	rx_packet_bytes = 0;
	ignore_bytes = 0;
	srcaddr_keep = 0;
}

void t2_300(void)
{
	if(tx_queue_next != 0)
	{
		//something to send
		
		if(PCIFR & _BV(PCIF2))
		{
			//this means that some transitions have happened since the last byte has been recieved. 
			//this SHOULD mean that a byte is being sent out at this very moment
			//but we haven't hit rx_isr yet because it isn't done
			//if this is the case, we should back off NOW before we turn on the transmitter or do anything
			
			PCIFR = _BV(PCIF2);
					
			OCR2A = TCNT2 + TICKS_150US;
			OCR2B = TCNT2 + TICKS_300US + (my_addr & 0x3F) * 2;
			return;
		}
		
		current_tx_queue_slot = tx_queue[0];
		tx_packet_bytes = 0;
		tx_packet_bytes_max = packet_queue_status[current_tx_queue_slot & 0x7F];
		
		//disable rx interrupt and enable tx interrupt
		UCSR0B = (UCSR0B & ~(_BV(RXCIE0))) | _BV(UDRIE0);
		tx_on();
	}
	
	//I think we need the next two lines, but I'm not sure
	OCR2A = TCNT2 + TICKS_150US;
	OCR2B = TCNT2 + TICKS_300US + (my_addr & 0x3F) * 2;
}

void uart_tx_isr(void)
{
	unsigned char c, i;
	unsigned char txerr;
	
	c = packet_queue[(current_tx_queue_slot & 0x7F) * MAX_PACKET_SIZE + tx_packet_bytes++];
	UDR0 = c;
	
	//there should not be any bytes before our own packet
	while(!(UCSR0A & _BV(TXC0)));	//this time, dead loop is OK; always will transmit
	
	//at this point, the byte should have been received already (maybe we need to wait? probably not)
	txerr = !(UCSR0A & _BV(RXC0));
	if(!txerr)
		txerr = UDR0 != c;	//this is because datasheet doesn't say what happens when we read empty fifo
	
	if(txerr)
	{
		//we failed to transmit our packet
		
		//enable rx interrupt and disable tx interrupt
		UCSR0B = (UCSR0B | _BV(RXCIE0)) & ~(_BV(UDRIE0));
		
		//fixme: do we need?
		//delay interrupt
		OCR2A = TCNT2 + TICKS_150US;
		OCR2B = TCNT2 + TICKS_300US + (my_addr & 0x3F) * 2;
		
		tx_off();
		
		return;
	}
	
	//we did transmit our byte so far
	if(tx_packet_bytes == tx_packet_bytes_max)
	{
		//we're done!
		if(!(current_tx_queue_slot & 0x80))
			queue_free(current_tx_queue_slot & 0x7F);
		//should be safe even if more stuff has been enqueued (we are atomic here)
		for(i = 0; i < tx_queue_next-1; i++)
			tx_queue[i] = tx_queue[i+1];
		tx_queue_next--;
		
		//enable rx interrupt and disable tx interrupt
		UCSR0B = (UCSR0B | _BV(RXCIE0)) & ~(_BV(UDRIE0));
		
		tx_off();
	}
	
	//delay interrupt
	OCR2A = TCNT2 + TICKS_150US;
	OCR2B = TCNT2 + TICKS_300US + (my_addr & 0x3F) * 2;
}
