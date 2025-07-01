helloWorld: hello.o
	ld -o hello hello.o -lSystem -syslibroot `xcrun --sdk macosx --show-sdk-path`  -e _start -arch arm64
mov_example: mov_example.o
		ld -o mov_example mov_example.o -lSystem -syslibroot `xcrun --sdk macosx --show-sdk-path`  -e _start -arch arm64
hello.o:
	as -arch arm64 -o hello.o hello.s
mov_example.o:
	as -arch arm64 -o mov_example.o mov_example.s
clean:
	rm hello hello.o mov_example mov_example.o
