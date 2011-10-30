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
	unsigned char csum;
	
	//this should run every 2.048 ms
	
	//what do we need to do?
	//1. discard datagrams we don't care about
	//2. handle incoming connections (if we are listening)
	//3. handle incoming data
	//4. handle ack logic
	
	//this is an isr, we are uinterruptable
	//cli should have been added in the appropriate places
	//so everything we see should be consistent
	
	//the first thing we want to do is increase the last packet interval on anything waiting for an ack
	for(i = 0; i < MAX_CONNECTIONS; i++)
	{
		if(conn_states[i].mode == 3)
		{
			conn_states[i].noack_time++;
		}
	}
	
	if(rx_queue_next != 0)
	{
		for(i = 0; i < rx_queue_next; i++)
		{
			//incoming packets in queue
			proto = packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 2];
			port = proto & 7;
			remoteport = (proto >> 3) & 7;
			remote = packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 0];
			
			if((proto & DATAGRAM_PROTOCOL_MASK) == DATAGRAM_PROTOCOL)
			{
				//a datagram
				
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
				
				//first thing: check checksum
				csum = doChecksum(&(packet_queue[rx_queue[i] * MAX_PACKET_SIZE]), packet_queue_status[rx_queue[i]]);
				if(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + packet_queue_status[rx_queue[i]] - 1] != csum)
				{
					//the checksum is wrong
					//let's just ignore it because
					//	a) there is no guarantee that the sender bytes are valid
					//  b) I'm too lazy to figure out the logic for wrong checksum on connection open/close
					//but we do need to dequeue it
					
					queue_free(rx_queue[i]);
					
					for(j = i; j < rx_queue_next-1; j++)
					{
						rx_queue[j] = rx_queue[j+1];
					}
					rx_queue_next--;
				}
				
				if(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 3] == STREAM_TYPE_OPEN_CONN)
				{
					//somebody wants to talk to us
					flag = 0;
					
					for(j = 0; j < MAX_CONNECTIONS; j++)
					{
						if((conn_states[j].mode != 0) && (((conn_states[j].ports >> 3) & 7) == port) &&
							((conn_states[j].ports & 7) == remoteport) && (conn_states[j].remote_addr == remote))
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
							
							//now we want to reset the connection
							conn_states[j].mode = 2;
							conn_states[j].tx_seq = 0;
							conn_states[j].rx_seq = 0;
							conn_states[j].tx_packet = 0xff;
							conn_states[j].rx_packet = 0xff;
							conn_states[j].noack_time = 0;
							//FIXME: There should be a way to tell the main program the connection was reset
							
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
					
					//dequeue the open packet (always)
					
					queue_free(rx_queue[i]);
					
					for(j = i; j < rx_queue_next-1; j++)
					{
						rx_queue[j] = rx_queue[j+1];
					}
					rx_queue_next--;
				}
				
				////////////////////////
				
				if(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 3] == STREAM_TYPE_PAYLOAD)
				{
					//somebody sent us data
					
					flag = 0;
					for(j = 0; j < MAX_CONNECTIONS; j++)
					{
						if((conn_states[j].mode == 2 || conn_states[j].mode == 3) && (((conn_states[j].ports >> 3) & 7) == port) &&
							((conn_states[j].ports & 7) == remoteport) && (conn_states[j].remote_addr == remote))
						{
							if((packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 4] != (conn_states[j].rx_seq & 0xFF)) || 
								(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 5] != ((conn_states[j].rx_seq >> 8) & 0xFF)))
							{
								//bad sequence
							
								slot = queue_alloc_isr();
								
								if(slot != -1)
								{
									packet_queue_status[slot] = 8;
									packet_queue[slot * MAX_PACKET_SIZE + 0] = my_addr;
									packet_queue[slot * MAX_PACKET_SIZE + 1] = conn_states[j].remote_addr;
									packet_queue[slot * MAX_PACKET_SIZE + 2] = conn_states[j].ports | STREAM_PROTOCOL;
									//payload now
									packet_queue[slot * MAX_PACKET_SIZE + 3] = STREAM_TYPE_NACK;
									packet_queue[slot * MAX_PACKET_SIZE + 4] = conn_states[j].rx_seq & 0xFF;
									packet_queue[slot * MAX_PACKET_SIZE + 5] = (conn_states[j].rx_seq >> 8) & 0xFF;
									packet_queue[slot * MAX_PACKET_SIZE + 6] = NACK_WRONGSEQ;
									packet_queue[slot * MAX_PACKET_SIZE + 7] = doChecksum(&(packet_queue[slot * MAX_PACKET_SIZE + 0]), 7);
									tx_queue[tx_queue_next++] = slot;
								}
								
								//dequeue the packet
								
								queue_free(rx_queue[i]);
								
								for(j = i; j < rx_queue_next-1; j++)
								{
									rx_queue[j] = rx_queue[j+1];
								}
								rx_queue_next--;
								
								flag = 1;
								break;
							}
						
							if(conn_states[j].rx_packet != 0xFF)
							{
								//no room
							
								slot = queue_alloc_isr();
								
								if(slot != -1)
								{
									packet_queue_status[slot] = 8;
									packet_queue[slot * MAX_PACKET_SIZE + 0] = my_addr;
									packet_queue[slot * MAX_PACKET_SIZE + 1] = conn_states[j].remote_addr;
									packet_queue[slot * MAX_PACKET_SIZE + 2] = conn_states[j].ports | STREAM_PROTOCOL;
									//payload now
									packet_queue[slot * MAX_PACKET_SIZE + 3] = STREAM_TYPE_NACK;
									packet_queue[slot * MAX_PACKET_SIZE + 4] = conn_states[j].rx_seq & 0xFF;
									packet_queue[slot * MAX_PACKET_SIZE + 5] = (conn_states[j].rx_seq >> 8) & 0xFF;
									packet_queue[slot * MAX_PACKET_SIZE + 6] = NACK_BUSY;
									packet_queue[slot * MAX_PACKET_SIZE + 7] = doChecksum(&(packet_queue[slot * MAX_PACKET_SIZE + 0]), 7);
									tx_queue[tx_queue_next++] = slot;
								}
							}
							else
							{
								conn_states[j].rx_packet = rx_queue[i];
								conn_states[j].rx_seq++;
							
								slot = queue_alloc_isr();
								
								if(slot != -1)
								{
									packet_queue_status[slot] = 8;
									packet_queue[slot * MAX_PACKET_SIZE + 0] = my_addr;
									packet_queue[slot * MAX_PACKET_SIZE + 1] = conn_states[j].remote_addr;
									packet_queue[slot * MAX_PACKET_SIZE + 2] = conn_states[j].ports | STREAM_PROTOCOL;
									//payload now
									packet_queue[slot * MAX_PACKET_SIZE + 3] = STREAM_TYPE_ACK;
									packet_queue[slot * MAX_PACKET_SIZE + 4] = conn_states[j].rx_seq & 0xFF;
									packet_queue[slot * MAX_PACKET_SIZE + 5] = (conn_states[j].rx_seq >> 8) & 0xFF;
									packet_queue[slot * MAX_PACKET_SIZE + 6] = ACK_STANDARD;
									packet_queue[slot * MAX_PACKET_SIZE + 7] = doChecksum(&(packet_queue[slot * MAX_PACKET_SIZE + 0]), 7);
									tx_queue[tx_queue_next++] = slot;
								}
								
								//remove from rx queue, but not the actual buffer
								for(j = i; j < rx_queue_next-1; j++)
								{
									rx_queue[j] = rx_queue[j+1];
								}
								rx_queue_next--;
							}
							flag = 1;
							break;
						}
					}
					if(!flag)
					{
						//no matched connection
							
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
							packet_queue[slot * MAX_PACKET_SIZE + 6] = NACK_NOTCONNECTED;
							packet_queue[slot * MAX_PACKET_SIZE + 7] = doChecksum(&(packet_queue[slot * MAX_PACKET_SIZE + 0]), 7);
							tx_queue[tx_queue_next++] = slot;
						}
					
						//dequeue the packet
						
						queue_free(rx_queue[i]);
						
						for(j = i; j < rx_queue_next-1; j++)
						{
							rx_queue[j] = rx_queue[j+1];
						}
						rx_queue_next--;
					}
				}
				
				if(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 3] == STREAM_TYPE_CLOSE_CONN)
				{
					for(j = 0; j < MAX_CONNECTIONS; j++)
					{
						if(conn_states[j].mode != 0 && (((conn_states[j].ports >> 3) & 7) == port) &&
							((conn_states[j].ports & 7) == remoteport) && (conn_states[j].remote_addr == remote))
						{
							//close the connection
							conn_states[j].mode = 0;
						}
					}
					
					//dequeue the packet (no matter if we matched anything)
					
					queue_free(rx_queue[i]);
					
					for(j = i; j < rx_queue_next-1; j++)
					{
						rx_queue[j] = rx_queue[j+1];
					}
					rx_queue_next--;
				}
				
				if(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 3] == STREAM_TYPE_ACK)
				{
					for(j = 0; j < MAX_CONNECTIONS; j++)
					{
						if(((packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 6] == ACK_STANDARD && conn_states[j].mode == 3) ||
							(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 6] == ACK_OPEN_CONN && conn_states[j].mode == 1)) &&
								(((conn_states[j].ports >> 3) & 7) == port) &&
							((conn_states[j].ports & 7) == remoteport) && (conn_states[j].remote_addr == remote))
						{
							//matched connection that is waiting for an ack
							
							if((packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 4] == (conn_states[j].rx_seq & 0xFF)) &&
								(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 5] == ((conn_states[j].rx_seq >> 8) & 0xFF)))
							{
								//and the sequence number is correct
								//note that an opening connection expects a seq of 0 and the seq will be 0 so we don't have to skip this check
								
								//then we can clear the wait for ack
								conn_states[j].mode = 2;
								conn_states[j].noack_time = 0;
								
								if(conn_states[j].tx_packet != 0xFF)
									queue_free(conn_states[j].tx_packet);	//now we don't need this
							}
						}
					}
					
					//dequeue the packet (no matter if we matched anything)
					
					queue_free(rx_queue[i]);
					
					for(j = i; j < rx_queue_next-1; j++)
					{
						rx_queue[j] = rx_queue[j+1];
					}
					rx_queue_next--;
				}
				
				if(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 3] == STREAM_TYPE_NACK)
				{
					for(j = 0; j < MAX_CONNECTIONS; j++)
					{
						if((((conn_states[j].ports >> 3) & 7) == port) &&
							((conn_states[j].ports & 7) == remoteport) && (conn_states[j].remote_addr == remote))
						{
							if(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 6] == NACK_BUSY)
							{
								//then we simply transmit it again
								if(conn_states[j].tx_packet != 0xFF)	//sanity check
									tx_queue[tx_queue_next++] = conn_states[j].tx_packet | 0x80;
							}
							else if(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 6] == NACK_WRONGSEQ)
							{
								//then we fix up the seq and send our packet again
								
								if(conn_states[j].tx_packet != 0xFF)	//sanity check
								{
									conn_states[j].tx_seq = (unsigned int)(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 4]) | (((unsigned int)(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 5])) << 8); 
									
									packet_queue[conn_states[j].tx_packet * MAX_PACKET_SIZE + 4] = conn_states[j].tx_seq & 0xFF;
									packet_queue[conn_states[j].tx_packet * MAX_PACKET_SIZE + 5] = (conn_states[j].tx_seq >> 8) & 0xFF;
									
									tx_queue[tx_queue_next++] = conn_states[j].tx_packet | 0x80;
								}
							}
							else if(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 6] == NACK_ALREADYOPEN)
							{
								if(conn_states[j].mode == 1)
								{
									//then this is ok actually
									
									conn_states[j].mode = 2;
									conn_states[j].noack_time = 0;
								}
							}
							else if(packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 6] == NACK_NOTLISTENING || packet_queue[rx_queue[i] * MAX_PACKET_SIZE + 6] == NACK_NOTCONNECTED)
							{
								//uh oh...
								conn_states[j].mode = 100;
							}
							else
							{
								//WTF?
								//do nothing
							}
						}
					}
					
					//dequeue the packet (no matter if we matched anything)
					
					queue_free(rx_queue[i]);
					
					for(j = i; j < rx_queue_next-1; j++)
					{
						rx_queue[j] = rx_queue[j+1];
					}
					rx_queue_next--;
				}
			}
		}
	}
}
