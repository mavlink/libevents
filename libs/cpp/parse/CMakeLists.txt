
add_library(libevents_parser
	parser.cpp
)
target_compile_options(libevents_parser
	PRIVATE
	-Wall
	-Wextra
	-Werror
	-Wconversion
	-Wpedantic
)

add_library(libevents_health_and_arming_checks
	health_and_arming_checks.cpp
)
target_compile_options(libevents_health_and_arming_checks
	PRIVATE
	-Wall
	-Wextra
	-Werror
	-Wconversion
	-Wpedantic
)
