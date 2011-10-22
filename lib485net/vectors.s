.section .init9,"ax",@progbits
;hack
ret

.section .vectors,"ax",@progbits

;we are (currently) only 4k, so rjmp works

.global _jumptable
_jumptable:
	rjmp initLib
	rjmp getVersion

	rjmp uart_rx_isr
	rjmp uart_tx_isr

	rjmp t2_150
	rjmp t2_300
	rjmp idle_isr

	rjmp setAddr

	rjmp sendRaw
	rjmp recvRaw
	rjmp peekPackets

	rjmp doChecksum

	rjmp listenDGram
	rjmp connectDGram
	rjmp sendDGram
	rjmp recvDGram
	rjmp closeDGram

	rjmp listenStream
	rjmp connectStream
	rjmp sendStream
	rjmp recvStream
	rjmp closeStream
