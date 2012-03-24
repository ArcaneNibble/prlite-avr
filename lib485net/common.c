#include "common.h"

unsigned char packet_queue[MAX_PACKET_SIZE * QUEUE_SIZE];
unsigned char packet_queue_status[QUEUE_SIZE];
unsigned char tx_queue[QUEUE_SIZE];
unsigned char rx_queue[QUEUE_SIZE];
unsigned char tx_queue_next;
unsigned char rx_queue_next;

unsigned long tx_bytes;
unsigned long rx_bytes;
unsigned long rx_overruns;
unsigned long protocol_errors;

unsigned char my_addr;

unsigned char multicast_groups[4];
