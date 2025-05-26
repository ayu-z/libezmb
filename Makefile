CC := gcc
CFLAGS := -Wall -Wextra -Werror -O2 -fPIC -shared 
LDFLAGS := -lzmq -lpthread -ljson-c -llua5.1


LIB_NAME := libezmb.so
LUA_LIB_NAME := e_device.so

SOURCES := e_proxy.c e_device.c \
           e_serial.c e_serialport.c \
           e_serial_config.c e_serial_manager.c \
           e_queue.c e_plugin_driver.c

OBJS := $(SOURCES:.c=.o)

LUA_SOURCES := e_device_lua.c e_device.c
LUA_OBJS := $(LUA_SOURCES:.c=.o)

INCLUDES += -I/usr/include/lua5.1


.PHONY: all lib lua clean install uninstall

all: lib lua


lib: $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(LIB_NAME) $(OBJS) $(LDFLAGS)


lua: $(LUA_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(LUA_LIB_NAME) $(LUA_OBJS) $(LDFLAGS)


%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@


clean:
	rm -f $(LIB_NAME) $(LUA_LIB_NAME) $(OBJS)


install:
	install -m 0755 $(LIB_NAME) /usr/lib/
	install -m 0755 $(LUA_LIB_NAME) /usr/local/lib/lua/5.1/
	install -d /usr/include/ezmb/
	install -m 0644 e_proxy.h e_device.h \
	                e_serialport.h e_serial_config.h \
	                e_serial_manager.h e_queue.h \
	                e_plugin_driver.h ezmb.h /usr/include/ezmb/

uninstall:
	rm -f /usr/lib/$(LIB_NAME)
	rm -f /usr/local/lib/lua/5.1/$(LUA_LIB_NAME)
	rm -rf /usr/include/ezmb
