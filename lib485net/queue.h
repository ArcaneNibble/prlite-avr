#ifndef QUEUE_H
#define QUEUE_H

extern unsigned char queue_alloc(void);
//this one doesn't do cli/sei
extern unsigned char queue_alloc_isr(void);

//this doesn't need cli/sei
extern void queue_free(unsigned char idx);

#endif
