#!/bin/bash
gcc -o libezmb.so e_proxy.c e_device.c -fPIC -shared -lzmq
gcc -fPIC -shared -o e_device.so e_device.c e_device_lua.c -lzmq -lpthread -I/usr/include/lua5.1 -I/usr/include/lua5.2 -I/usr/include/lua5.3 -I/usr/local/include/lua -llua5.1
# gcc -o proxy proxy.c -L . -lemb
# export LD_LIBRARY_PATH=./:$LD_LIBRARY_PATH
# socat -d -d pty,raw,echo=0,link=/tmp/virtual_serial1 tcp-listen:12345,reuseaddr
# socat -d -d pty,raw,echo=0,link=/tmp/virtual_serial2 tcp-listen:12346,reuseaddr
gcc -o test test.c -L . -lezmb
gcc -o client client.c -L . -lezmb
gcc -o e_serial e_serial.c e_serialport.c e_serial_config.c e_serial_manager.c e_queue.c -L . -lezmb -lpthread -ljson-c
# gcc -o e_config e_config.c e_queue.c -lpthread -ljson-c
gcc -o e_serial_coll e_serial_coll.c  e_serialport.c e_serial_config.c e_serial_manager.c e_queue.c -lpthread -ljson-c -L . -lezmb 
gcc -o monitor monitor.c -L . -lezmb