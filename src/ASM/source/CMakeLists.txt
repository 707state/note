cmake_minimum_required(VERSION 3.16)
project(HelloAssembly)

enable_language(ASM)
set(CMAKE_OSX_ARCHITECTURES arm64)
set(CMAKE_ASM_FLAGS "-g")
function(add_asm_exe TARGET)
	cmake_parse_arguments(ARG
		"EXE;LIB"
		""
		"LIBS;LINK;COMPILE;PLATFORM"
		${ARGN})
	set(SOURCES ${ARG_UNPARSED_ARGUMENTS})
	if(ARG_EXE)
		add_executable(${TARGET} ${SOURCES})
	elseif(ARG_LIB)
		add_library(${TARGET} ${SOURCES})
	else()
		message(FATAL_ERROR "Must specify EXE or LIB for target ${TARGET}")
	endif()

	if(ARG_COMPILE)
		target_compile_options(${TARGET} PRIVATE ${ARG_COMPILE})
	endif()

	if(ARG_LIBS)
		target_link_libraries(${TARGET} PUBLIC ${ARG_LIBS})
	endif()

	if(NOT ARG_PLATFORM)
		if(APPLE)
			set(ARG_PLATFORM "Apple")
		elseif(UNIX)
			set(ARG_PLATFORM "Linux")
		elseif(WIN32)
			set(ARG_PLATFORM "Windows")
		endif()
	endif()

	if(ARG_PLATFORM STREQUAL "Apple")
		message(STATUS "Configuring ${TARGET} for Apple arm64")
		if(ARG_LINK)
			set_target_properties(${TARGET} PROPERTIES 
				LINK_FLAGS "${ARG_LINK}")
		endif()
	endif()
endfunction()
include_directories(.)
add_asm_exe(hello
	hello.s
	EXE
	COMPILE "-g"
	LINK "-e _start"
	PLATFORM "Apple")

add_asm_exe(mov_example
	mov_example.s
	EXE
	COMPILE "-g"
	LINK "-e _start"
	PLATFORM "Apple")

add_asm_exe(add_example
	add_example.s
	EXE 
	COMPILE "-g"
	LINK "-e _start"
	PLATFORM "Apple")

add_asm_exe(add_example2
	add_example2.s
	EXE
	COMPILE "-g"
	LINK "-e _start"
	PLATFORM "Apple")
add_asm_exe(loop_example
	./loop_example.s
	EXE
	COMPILE "-g"
	LINK "-e _start"
	PLATFORM "Apple")
add_asm_exe(switch_case
	./switch_case.s
	EXE
	COMPILE "-g"
	LINK "-e _start"
	PLATFORM "Apple")
add_asm_exe(upper
	upper.s
	EXE
	COMPILE "-g"
	LINK "-e _start"
	PLATFORM "Apple")
add_asm_exe(toupper
	toupper.s
	LIB
	COMPILE "-g")

add_asm_exe(main_toupper
	main.s
	EXE
	COMPILE "-g"
	LIBS toupper "-e _start"
	PLATFORM "Apple")
add_asm_exe(main_macro
	main_macro.s
	EXE
	COMPILE "-g"
	LIBS "-e _start"
	PLATFORM "Apple")
add_asm_exe(main_file
	main_file.s
	EXE
	COMPILE "-g"
	LINK "-e _start"
	LIBS toupper)
add_asm_exe(matrix_mult_neon
	matrix_mult_neon.s
	EXE
	COMPILE "-g"
	LINK "-e main")
