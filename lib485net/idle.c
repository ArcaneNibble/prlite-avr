#include "util.h"
#include "common.h"
#include "queue.h"

void idle_isr(void) __attribute__((signal));

void idle_isr(void)
{
	//what do we need to do?
	//1. discard datagrams we don't care about
	//2. handle incoming connections (if we are listening)
	//3. handle incoming data
	//4. handle ack logic
	
	
}
