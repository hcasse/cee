
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# configuration
set(PROGRAM		"cee")
set(SOURCES		"cee.cpp")

# look for OTAWA
if(NOT OTAWA_CONFIG)
    find_program(OTAWA_CONFIG otawa-config DOC "path to otawa-config")
    if(NOT OTAWA_CONFIG)
        message(FATAL_ERROR "ERROR: otawa-config is required !")
    endif()
endif()
message(STATUS "otawa-config found at ${OTAWA_CONFIG}")
execute_process(COMMAND "${OTAWA_CONFIG}" --cflags ${MODULES} OUTPUT_VARIABLE OTAWA_CFLAGS  OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND "${OTAWA_CONFIG}" --libs -r ${MODULES}   OUTPUT_VARIABLE OTAWA_LDFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND "${OTAWA_CONFIG}" --prefix            OUTPUT_VARIABLE OTAWA_PREFIX  OUTPUT_STRIP_TRAILING_WHITESPACE)

# program installation
set(ORIGIN $ORIGIN)
set(CMAKE_INSTALL_RPATH "${ORIGIN}/../../../")
include_directories("${CMAKE_SOURCE_DIR}" ".")
add_executable(${PROGRAM} ${SOURCES})
set_property(TARGET ${PROGRAM} PROPERTY PREFIX "")
set_property(TARGET ${PROGRAM} PROPERTY COMPILE_FLAGS "${OTAWA_CFLAGS}")
target_link_libraries(${PROGRAM} "${OTAWA_LDFLAGS}")
