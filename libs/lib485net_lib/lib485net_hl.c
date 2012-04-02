#include "lib485net_hl.h"

unsigned char recvJumboDGram(unsigned char *packetin, unsigned char lenin, unsigned char type, unsigned char addr, unsigned char **packetout, unsigned char *lenout)
{
	jumbo_dgram_entry *e;
	
	//packetin is data only
	e = (jumbo_dgram_entry *)(packetin);
	
	while(1)
	{
		if((unsigned char *)(e) >= (packetin + lenin))
		{
			*packetout = 0;
			*lenout = 0;
			return 1;	//out of range
		}
		if(e->type == type && e->addr == addr)
		{
			//good, what we want
			*packetout = (unsigned char *)(&(e->data[0]));
			if(e->next != 0)
				*lenout = e->next - 3;		//hardcoded!
			else
				*lenout = packetin + lenin - (unsigned char *)(e) - 3;
			return 0;
		}
		
		if(e->next == 0)
			break;
		e = (jumbo_dgram_entry *)((unsigned char *)(e) + e->next);
	}
	
	*packetout = 0;
	*lenout = 0;
	return 1;	//not found
}
