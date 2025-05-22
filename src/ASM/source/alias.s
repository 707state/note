.macro PUSH1 register
	STR \register,[SP,#-16]!
.endm
.macro POP1 register
	LDR \register,[sp],#16
.endm
.macro PUSH2 register1,register2
	STP \register1,\register2,[sp,#-16]!
.endm
.macro POP2 register1,register2
	LDP \register1,\register2,[sp],#16
.endm
