cmake_minimum_required(VERSION 3.10)
project(e_tcp_server_act C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -O2")


find_package(PkgConfig REQUIRED)
pkg_check_modules(EVENT REQUIRED libevent)
pkg_check_modules(EZMB REQUIRED libezmb)

include_directories(
    ${EVENT_INCLUDE_DIRS}
    ${EZMB_INCLUDE_DIRS}
)

link_directories(
    ${EVENT_LIBRARY_DIRS}
    ${EZMB_LIBRARY_DIRS}
)

set(SOURCES
    main.c
    e_tcp_server.c
    e_tcp_server_config.c
)

add_executable(e_tcp_server_act ${SOURCES})
target_link_libraries(e_tcp_server_act ${EVENT_LIBRARIES} ${EZMB_LIBRARIES})
