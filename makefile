
EXE = fred
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic
BUILD_DIR = ./build
DEBUG_DIR = ./debug
OBJS = $(BUILD_DIR)/main.o
DEBUG_OBJS = $(DEBUG_DIR)/main.o
FILES = $(wildcard src/*.c) $(wildcard src/*.h)
# FILES = src/%.c src/%.h -> i guess % only works inside the actual target or prerequisites 

.PHONY = all clean clean_debug $(DEBUG_DIR)/fred

all: $(BUILD_DIR)/$(EXE) 


$(BUILD_DIR)/$(EXE) : $(OBJS) 
	$(CC) -o $@ $< $(CFLAGS) 

$(BUILD_DIR)/%.o : $(FILES)
	mkdir -p $(BUILD_DIR)
	$(CC) -c -o $@ $< $(CFLAGS)



$(DEBUG_DIR)/$(EXE): $(DEBUG_OBJS)
	$(CC) -g -o $@ $< $(CFLAGS) 

$(DEBUG_DIR)/%.o: $(FILES) | $(DEBUG_DIR) # order-only-prerequisites
	$(CC) -g -c -o $@ $< $(CFLAGS)

$(DEBUG_DIR): 
	mkdir $(DEBUG_DIR)



clean:
	$(RM) $(BUILD_DIR)/*

clean_debug:
	$(RM) $(DEBUG_DIR)/*


# https://stackoverflow.com/questions/5178125/how-to-place-object-files-in-separate-subdirectory

# debugging with gdb:  https://stackoverflow.com/questions/8963208/gdb-display-output-of-target-application-in-a-separate-window
# debuggin with gdbserver: https://stackoverflow.com/a/15306382 (prefer this, for some reason using redirecting with tty 
# causes the output terminal to crash sometimes)
#
