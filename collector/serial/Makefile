CC := gcc
CFLAGS := -Wall -Wextra -Werror -O2

LIBS := -lezmb

SOURCES = main.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = e_serial_coll

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
