CC = gcc
CFLAGS = -c -Wall -Os -pipe
CPPFLAGS =
LDFLAGS = -pthread
SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = rssdcc-bot

all: $(SOURCES) $(EXECUTABLE)

debug: CFLAGS += -g -Wextra
debug: CPPFLAGS += -DENABLE_DEBUG
debug: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	@echo "  LD        $@"
	@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.c.o:
	@echo "  CC        $@"
	@$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@

clean:
	@rm -f $(OBJECTS) $(EXECUTABLE)
