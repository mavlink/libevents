
add_library(libevents_receive
	receive.cpp
)
target_compile_options(libevents_receive
	PRIVATE
	-Wall
	-Wextra
	-Werror
	-Wconversion
	-Wpedantic
)

