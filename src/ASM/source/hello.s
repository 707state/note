.global _start
_start: mov X0,#1
	adr X1,helloWorld // string to print
	mov X2,#13 // length of string
	mov X16,#4 // Unix write system call
	svc #0x80 // call kernel to output the string
// setup parameter to exit program
	mov X0,#-2 // use 0 as return value
	mov X16,#1 // syscall 1 ti termiate
	svc #0x80 // call kernel to exit
helloWorld: .ascii "Hello World!\n"
	
