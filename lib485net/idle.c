#include "util.h"
#include "common.h"
#include "queue.h"
#include "packetDatagram.h"
#include "packetStream.h"

void idle_isr(void) __attribute__((signal));

void idle_isr(void)
{
	unsigned char i,j;
	unsigned char proto;
	unsigned char port, remoteport;
	unsigned char remote;
	unsigned char flag;
	unsigned char slot;
	//what do we need to do?
	//1. discard datagrams we don't care about
	//2. handle incoming connections (if we are listening)
	//3. handle incoming data
	//4. handle ack logic
	
	//this is an isr, we are uinterruptable
	//cli should have been added in the appropriate places
	//so everything we see should be consistent
	
	if(rx_queue_next != 0)
	{
		for(i = 0; i < rx_queue_next; i++)
		{
			//incoming packets in queue
			proto = packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 2];
			
			if((proto & DATAGRAM_PROTOCOL_MASK) == DATAGRAM_PROTOCOL)
			{
				//a datagram
				port = proto & 7;
				
				if(!open_dgram_ports[port])
				{
					//we don't care about this port
					queue_free(rx_queue[i]);
					
					for(j = i; j < rx_queue_next-1; j++)
					{
						rx_queue[j] = rx_queue[j+1];
					}
					rx_queue_next--;
				}
			}
			
			///////////////////////////////////////////////////////////////////
			
			if((proto & STREAM_PROTOCOL_MASK) == STREAM_PROTOCOL)
			{
				//stream thing
				if(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 3] == STREAM_TYPE_OPEN_CONN)
				{
					//somebody wants to talk to us
					port = proto & 7;
					remoteport = (proto >> 3) & 7;
					remote = packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 0];
					flag = 0;
					
					for(j = 0; j < MAX_CONNECTIONS; j++)
					{
						if((conn_states[j].mode != 0) && (((conn_states[j].ports >> 3) & 7) == port) &&
							((conn_states[j].ports & 7) == remoteport) &&(conn_states[j].remote_addr == remote))
						{
							//we are already connected
							
							slot = queue_alloc_isr();
							
							if(slot != -1)
							{
								packet_queue_status[slot] = 8;
								packet_queue[slot * MAX_PACKET_SIZE + 0] = my_addr;
								packet_queue[slot * MAX_PACKET_SIZE + 1] = conn_states[j].remote_addr;
								packet_queue[slot * MAX_PACKET_SIZE + 2] = conn_states[j].ports | STREAM_PROTOCOL;
								//payload now
								packet_queue[slot * MAX_PACKET_SIZE + 3] = STREAM_TYPE_NACK;
								packet_queue[slot * MAX_PACKET_SIZE + 4] = 0;
								packet_queue[slot * MAX_PACKET_SIZE + 5] = 0;
								packet_queue[slot * MAX_PACKET_SIZE + 6] = NACK_ALREADYOPEN;
								packet_queue[slot * MAX_PACKET_SIZE + 7] = doChecksum(&(packet_queue[slot * MAX_PACKET_SIZE + 0]), 7);
								tx_queue[tx_queue_next++] = slot;
							}
							
							flag = 1;
							break;
						}
						else if((conn_states[j].mode == 4) && (((conn_states[j].ports >> 3) & 7) == port))
						{
							//yay, we care
							flag = 1;
							conn_states[j].mode = 2;
							
							conn_states[j].remote_addr = remote;
							conn_states[j].ports |= remoteport;
							
							slot = queue_alloc_isr();
							
							if(slot != -1)
							{
								packet_queue_status[slot] = 8;
								packet_queue[slot * MAX_PACKET_SIZE + 0] = my_addr;
								packet_queue[slot * MAX_PACKET_SIZE + 1] = conn_states[j].remote_addr;
								packet_queue[slot * MAX_PACKET_SIZE + 2] = conn_states[j].ports | STREAM_PROTOCOL;
								//payload now
								packet_queue[slot * MAX_PACKET_SIZE + 3] = STREAM_TYPE_ACK;
								packet_queue[slot * MAX_PACKET_SIZE + 4] = 0;
								packet_queue[slot * MAX_PACKET_SIZE + 5] = 0;
								packet_queue[slot * MAX_PACKET_SIZE + 6] = ACK_OPEN_CONN;
								packet_queue[slot * MAX_PACKET_SIZE + 7] = doChecksum(&(packet_queue[slot * MAX_PACKET_SIZE + 0]), 7);
								tx_queue[tx_queue_next++] = slot;
							}
							
							flag = 1;
							break;
						}
					}
					
					if(!flag)
					{
						//we didn't care
							
						slot = queue_alloc_isr();
						
						if(slot != -1)
						{
							packet_queue_status[slot] = 8;
							packet_queue[slot * MAX_PACKET_SIZE + 0] = my_addr;
							packet_queue[slot * MAX_PACKET_SIZE + 1] = remote;
							packet_queue[slot * MAX_PACKET_SIZE + 2] = remoteport | (port << 3) | STREAM_PROTOCOL;
							//payload now
							packet_queue[slot * MAX_PACKET_SIZE + 3] = STREAM_TYPE_NACK;
							packet_queue[slot * MAX_PACKET_SIZE + 4] = 0;
							packet_queue[slot * MAX_PACKET_SIZE + 5] = 0;
							packet_queue[slot * MAX_PACKET_SIZE + 6] = NACK_NOTLISTENING;
							packet_queue[slot * MAX_PACKET_SIZE + 7] = doChecksum(&(packet_queue[slot * MAX_PACKET_SIZE + 0]), 7);
							tx_queue[tx_queue_next++] = slot;
						}
					}
				}
			}
		}
	}
}
