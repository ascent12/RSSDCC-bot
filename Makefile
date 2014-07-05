CC = gcc
CFLAGS = -c -Wall -O2
CPPFLAGS =
LDFLAGS =
SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = rssdcc-bot

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	@echo "  LD        $@"
	@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.c.o:
	@echo "  CC        $@"
	@$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@

clean:
	@rm -f $(OBJECTS) $(EXECUTABLE)
