.global _start
.align 4

_start:
	mov x2,#0x0000000000000003
	mov x3,#0xFFFFFFFFFFFFFFFF
	mov x4,#0x0000000000000005
	mov x5,#0x0000000000000001
	adds x1,x3,x5
	adc x0,x2,x4
	mov X16, #1
	svc #0x80
