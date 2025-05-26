cmake_minimum_required(VERSION 3.10)
project(e_serial_coll C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -O2")


find_package(PkgConfig REQUIRED)
pkg_check_modules(EZMB REQUIRED libezmb)

include_directories(
    ${EZMB_INCLUDE_DIRS}
)

link_directories(
    ${EZMB_LIBRARY_DIRS}
)

set(SOURCES
    main.c
)

add_executable(e_serial_coll ${SOURCES})
target_link_libraries(e_serial_coll ${EVENT_LIBRARIES} ${EZMB_LIBRARIES})
