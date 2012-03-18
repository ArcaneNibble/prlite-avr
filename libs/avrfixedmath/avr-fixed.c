#include "avr-fixed.h"

FIXED1616 fixed_mult(FIXED1616 inArg0, FIXED1616 inArg1)
{
	register unsigned long _a = (inArg0 >= 0) ? inArg0 : (-inArg0);
	register unsigned long _b = (inArg1 >= 0) ? inArg1 : (-inArg1);
	unsigned char needsflipsign = (inArg0 >= 0) != (inArg1 >= 0);
	
	register FIXED1616 result;
	
	asm volatile(
		"eor %A[result], %A[result]"								"\n\t"
		"eor %B[result], %B[result]"								"\n\t"
		"eor %C[result], %C[result]"								"\n\t"
		"eor %D[result], %D[result]"								"\n\t"
		"eor r18, r18"												"\n\t"
		"eor r19, r19"												"\n\t"
		"eor r20, r20"												"\n\t"
		"eor r21, r21"												"\n\t"
		"eor r22, r22"												"\n\t"
		
		// i = 5
		"mul %C[argA], %D[argB]"									"\n\t"
		"mov %A[result], r0"										"\n\t"
		"mov %B[result], r1"										"\n\t"
		"mul %D[argA], %C[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mov %D[result], %C[result]"								"\n\t"
		"mov %C[result], %B[result]"								"\n\t"
		"mov %B[result], %A[result]"								"\n\t"
		"mov %A[result], r22"										"\n\t"
		
		// i = 4
		"mul %B[argA], %D[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mul %C[argA], %C[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mul %D[argA], %B[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mov %D[result], %C[result]"								"\n\t"
		"mov %C[result], %B[result]"								"\n\t"
		"mov %B[result], %A[result]"								"\n\t"
		"mov %A[result], r22"										"\n\t"
		
		// i = 3
		"mul %A[argA], %D[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mul %B[argA], %C[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mul %C[argA], %B[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mul %D[argA], %A[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mov %D[result], %C[result]"								"\n\t"
		"mov %C[result], %B[result]"								"\n\t"
		"mov %B[result], %A[result]"								"\n\t"
		"mov %A[result], r22"										"\n\t"
		
		// i = 2
		"mul %A[argA], %C[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mul %B[argA], %B[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"mul %C[argA], %A[argB]"									"\n\t"
		"add %A[result], r0"										"\n\t"
		"adc %B[result], r1"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		
		// i = 1
		"mul %A[argA], %B[argB]"									"\n\t"
		"mov r18, r0"												"\n\t"
		"mov r19, r1"												"\n\t"
		"mul %B[argA], %A[argB]"									"\n\t"
		"add r18, r0"												"\n\t"
		"adc r19, r1"												"\n\t"
		"adc r20, r22"												"\n\t"
		"mov r21, r20"												"\n\t"
		"mov r20, r19"												"\n\t"
		"mov r19, r18"												"\n\t"
		"mov r18, r22"												"\n\t"
		
		// i = 0
		"mul %A[argA], %A[argB]"									"\n\t"
		"add r18, r0"												"\n\t"
		"adc r19, r1"												"\n\t"
		"adc r20, r22"												"\n\t"
		
		//low += 0x8000;
		"ldi r22, 0x80"												"\n\t"
		"add r19, r22"												"\n\t"
		"eor r22, r22"												"\n\t"
		"adc r20, r22"												"\n\t"
		
		//mid += (low >> 16);
		"add %A[result], r20"										"\n\t"
		"adc %B[result], r21"										"\n\t"
		"adc %C[result], r22"										"\n\t"
		"adc %D[result], r22"										"\n\t"
		
		"eor r1, r1"												"\n\t"

		: [result] "=&r" (result)
		: [argA] "r" (_a),	[argB] "r" (_b)
		: "r18", "r19", "r20", "r21",		"r22"
	);

	/* Figure out the sign of result */
	if (needsflipsign)
	{
		result = -result;
	}

	return result;
}
