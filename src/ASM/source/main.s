.global _start
.p2align 2
_start:
	adrp x0,instr@PAGE
	add x0,x0,instr@PAGEOFF
	adrp x1,outstr@PAGE
	add x1,x1,outstr@PAGEOFF
	bl toupper // call toupper 
	mov x2,x0
	mov x0,#1
	adrp x1,outstr@PAGE
	add x1,x1,outstr@PAGEOFF
	mov x16,#4
	svc #0x80
	mov x0,#0
	mov x16,#1
	svc #0x80
.data
instr: .asciz "This is out Testing String that we will convert.\n"
outstr: .fill 255,1,0
	
