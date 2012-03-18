#ifndef AVR_FIXED_H
#define AVR_FIXED_H

typedef signed long FIXED1616;

static inline FIXED1616 int_to_fixed(int i)
{
	return ((FIXED1616)i) << 16;
}

static inline int fixed_to_int(FIXED1616 i)
{
	return i >> 16;
}

extern FIXED1616 fixed_mult(FIXED1616 inArg0, FIXED1616 inArg1);

#endif
