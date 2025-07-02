.global toupper
.align 4
toupper: 
	mov x4,x1
	loop:
	ldrb w5,[x0],#1
	cmp w5,#'z'
	b.gt cont
	cmp w5,#'a'
	b.lt cont
	sub w5,w5,#('a'-'A')
	cont:
	strb w5,[x1],#1
	cmp w5,#0
	b.ne loop
	sub x0,x1,x4
	ret
