CC      = cc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -O2
SRCS    = src/gen.c src/template.c
TARGET  = gen

.PHONY: all build run clean

## Build the generator binary.
all: build

build: $(TARGET)

$(TARGET): $(SRCS) src/template.h
	$(CC) $(CFLAGS) -I src -o $@ $(SRCS)

## Build and regenerate output/index.html.
run: build
	./$(TARGET)

## Remove the binary.
clean:
	rm -f $(TARGET)
