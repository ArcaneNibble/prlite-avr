.section .init9,"ax",@progbits
;hack
ret

.section .vectors,"ax",@progbits

.global _jumptable
_jumptable:
	jmp initLib
	jmp getVersion

	jmp uart_rx_isr
	jmp uart_tx_isr

	jmp t2_150
	jmp t2_300
	jmp idle_isr

	jmp setAddr

	jmp sendRaw
	jmp recvRaw
	jmp peekPackets

	jmp doChecksum

	jmp listenDGram
	jmp connectDGram
	jmp sendDGram
	jmp recvDGram
	jmp closeDGram

	jmp listenStream
	jmp connectStream
	jmp sendStream
	jmp recvStream
	jmp closeStream

	#new in 0.1.1
	jmp recvDGramLL

	#new in 0.3.0
	jmp setMulticast
