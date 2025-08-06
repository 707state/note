.include "macro_toupper.s"
.global _start
.align 4
_start:
	mov x2,x0
	mov x0,#1
	adrp x1,buffer@PAGE
	add x1,x1,buffer@PAGEOFF
	mov x16,#4
	svc #0x80
	toupper tststr2,buffer
	mov x2,x0
	mov x0,#1
	adrp x1,buffer@PAGE
	add x1,x1,buffer@PAGEOFF
	mov x16,#4
	svc #0x80
	mov x0,#0
	mov x16,#1
	svc #0x80
.data
tststr: .asciz "This is our Test String that we will convert!\n"
tststr2: .asciz "A second string to upper case!!\n"
buffer: .fill 255,1,0
