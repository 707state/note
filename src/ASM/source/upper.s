.global _start
.p2align 2
_start:
	adrp x4,instr@PAGE
	add x4,x4,instr@PAGEOFF
	adrp x3,outstr@PAGE
	add x3,x3,outstr@PAGEOFF
loop:
	ldrb w5,[x4],#1 // load character and increment pointer
	cmp w5,#'z'
	b.gt cont
	cmp w5,#'a'
	b.lt cont
	sub w5,w5,#('a'-'A')
cont:
	strb w5,[x3],#1
	cmp w5,#0 // 与'\0'进行比较，用来判断是否走到了字符串的结尾
	b.ne loop
	mov x0,#1 // StdOut的syscall
	adrp x1,outstr@PAGE
	add x1,x1,outstr@PAGEOFF
	sub x2,x3,x1
	mov x16,#4 //write
	svc #0x80
	mov x0,#0
	mov x16,#1
	svc #0x80
.data
instr: .asciz "This is our Test String that we will convert.\n"
outstr: .fill 255,1,0
