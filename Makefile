CC ?= clang 
CFLAGS += -DLINUX -g -Wall -I. -lcurl 

LIBPATH = -L.
LDFLAGS += $(LIBPATH) -lcurl 

EXECUTABLE=samk

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)


all: $(EXECUTABLE)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	-rm -f $(EXECUTABLE) $(OBJECTS) *~

