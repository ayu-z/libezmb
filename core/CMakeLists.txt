cmake_minimum_required(VERSION 3.10)
project(ezmb C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -O2 -fPIC")


find_package(PkgConfig REQUIRED)
pkg_check_modules(LUA REQUIRED lua5.1)
pkg_check_modules(ZMQ REQUIRED libzmq)
pkg_check_modules(JSONC REQUIRED json-c)

include_directories(
    ${LUA_INCLUDE_DIRS}
    ${ZMQ_INCLUDE_DIRS}
    ${JSONC_INCLUDE_DIRS}
)

link_directories(
    ${LUA_LIBRARY_DIRS}
    ${ZMQ_LIBRARY_DIRS}
    ${JSONC_LIBRARY_DIRS}
)

set(LIB_SOURCES
    e_proxy.c
    e_device.c
    e_serialport.c
    e_serial_config.c
    e_serial_manager.c
    e_queue.c
    e_plugin_driver.c
)

add_library(ezmb SHARED ${LIB_SOURCES})
target_link_libraries(ezmb ${ZMQ_LIBRARIES} ${JSONC_LIBRARIES} ${LUA_LIBRARIES} pthread)

install(TARGETS ezmb LIBRARY DESTINATION /usr/lib)
install(FILES
    e_proxy.h
    e_device.h
    e_serialport.h
    e_serial_config.h
    e_serial_manager.h
    e_queue.h
    e_plugin_driver.h
    ezmb.h
    DESTINATION /usr/include/ezmb
)
install(FILES
    libezmb.pc
    DESTINATION /usr/lib/pkgconfig
)

add_library(e_device MODULE e_device_lua.c e_device.c)
set_target_properties(e_device PROPERTIES PREFIX "" OUTPUT_NAME "e_device")
target_link_libraries(e_device ${ZMQ_LIBRARIES} ${LUA_LIBRARIES} pthread)

install(TARGETS e_device LIBRARY DESTINATION /usr/local/lib/lua/5.1)

