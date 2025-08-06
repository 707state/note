.global _start
.align 4
// Load X2 with 0x1234fedc4f5d6e3a first using MOV and MOVK
_start: mov X2,#0x6e3a
	movk x2,#0x4f5d, LSL #16
	MOVK X2, #0xFEDC, LSL #32
	MOVK X2, #0x1234, LSL #48
	MOV w1,w2 // move W2 into W1
	LSL x1,x2,#1 // loginal shift left
	LSR x1,x2,#1
	ASR X1,X2,#1
	ROR x1,x2,#1
	MOV x1,#0xAB000000
	MOVN W1,#45
	MOV W1,#0xFFFFFFFE
	MOV X0,#3
	MOV X16,#1
	SVC #0x80
