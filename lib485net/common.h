#ifndef COMMON_H
#define COMMON_H

//not payload, all of it
#define MAX_PACKET_SIZE		64
//number of packets
#define QUEUE_SIZE			8

//actual incoming bytes
extern unsigned char packet_queue[];
//size of packets (0 = unallocated)
extern unsigned char packet_queue_status[];
//index into queue
extern unsigned char tx_queue[];
extern unsigned char rx_queue[];
extern unsigned char tx_queue_next;
extern unsigned char rx_queue_next;

extern unsigned long tx_bytes;
extern unsigned long rx_bytes;
extern unsigned long rx_overruns;
extern unsigned long protocol_errors;

extern unsigned char my_addr;

#endif
