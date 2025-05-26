CC := gcc
CFLAGS := -Wall -Wextra -Werror -O2

LIBS := -levent -lezmb

SOURCES = main.c e_tcp_server.c e_tcp_server_config.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = e_tcp_server_act

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
