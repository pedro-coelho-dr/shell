CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lpthread

OUTPUT = shell
SRC_FILE = shell.c
HDR_FILE = shell.h

all: $(OUTPUT)

$(OUTPUT): $(SRC_FILE) $(HDR_FILE)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SRC_FILE) $(LDFLAGS)

clean:
	rm -f $(OUTPUT)

debug: $(SRC_FILE) $(HDR_FILE)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SRC_FILE) $(LDFLAGS)
	gdb ./$(OUTPUT)

run: $(OUTPUT)
	./$(OUTPUT)

.PHONY: all clean debug run