.section .jumps,"ax",@progbits

.global _jumptable
_jumptable:
	rjmp tx_on
	rjmp tx_off
	rjmp rx_on
	rjmp rx_off
