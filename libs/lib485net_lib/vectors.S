#define LIB_ORIGIN 0x5000

.global initLib
.set initLib, LIB_ORIGIN + 0 * 4
.global getVersion
.set getVersion, LIB_ORIGIN + 1 * 4

.global uart_rx_isr
.set uart_rx_isr, LIB_ORIGIN + 2 * 4
.global uart_tx_isr
.set uart_tx_isr, LIB_ORIGIN + 3 * 4

//
.global __vector_18
.set __vector_18, uart_rx_isr
.global __vector_19
.set __vector_19, uart_tx_isr
//

.global t2_150
.set t2_150, LIB_ORIGIN + 4 * 4
.global t2_300
.set t2_300, LIB_ORIGIN + 5 * 4
.global idle_isr
.set idle_isr, LIB_ORIGIN + 6 * 4

//
.global __vector_9
.set __vector_9, idle_isr
.global __vector_8
.set __vector_8, t2_300	//B
.global __vector_7
.set __vector_7, t2_150	//A
//

.global setAddr
.set setAddr, LIB_ORIGIN + 7 * 4

.global sendRaw
.set sendRaw, LIB_ORIGIN + 8 * 4
.global recvRaw
.set recvRaw, LIB_ORIGIN + 9 * 4
.global peekPackets
.set peekPackets, LIB_ORIGIN + 10 * 4

.global doChecksum
.set doChecksum, LIB_ORIGIN + 11 * 4

.global listenDGram
.set listenDGram, LIB_ORIGIN + 12 * 4
.global connectDGram
.set connectDGram, LIB_ORIGIN + 13 * 4
.global sendDGram
.set sendDGram, LIB_ORIGIN + 14 * 4
.global recvDGram
.set recvDGram, LIB_ORIGIN + 15 * 4
.global closeDGram
.set closeDGram, LIB_ORIGIN + 16 * 4

.global listenStream
.set listenStream, LIB_ORIGIN + 17 * 4
.global connectStream
.set connectStream, LIB_ORIGIN + 18 * 4
.global sendStream
.set sendStream, LIB_ORIGIN + 19 * 4
.global recvStream
.set recvStream, LIB_ORIGIN + 20 * 4
.global closeStream
.set closeStream, LIB_ORIGIN + 21 * 4