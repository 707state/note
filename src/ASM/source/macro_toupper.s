.macro toupper instr,outstr
	adrp x0,\instr@PAGE
	add x0,x0,\instr@PAGEOFF
	adrp x1,\outstr@PAGE
	add x1,x1,\outstr@PAGEOFF
	mov x2,x1
1:
	ldrb w3,[x0],#1
	cmp w3,#'z'
	b.gt 2f
	cmp w3,#'a'
	b.lt 2f
	sub w3,w3,#('a'-'A')
2:
	strb w3,[x1],#1
	cmp w3,#0
	b.ne 1b
	sub x0,x1,x2
.endm
