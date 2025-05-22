
EXE = fred
CC = gcc 
CFLAGS = -Wall -Wextra -Wpedantic -Wno-comment
BUILD_DIR = ./build
DEBUG_DIR = ./debug
TEST_DIR = ./tests

OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/fred.o
DEBUG_OBJS = $(DEBUG_DIR)/main.o  $(DEBUG_DIR)/fred.o

.PHONY = all                \
         clean              \
         clean_debug        \
         $(DEBUG_DIR)/fred  \
				 $(TEST_DIR)/test 

all: $(BUILD_DIR)/$(EXE)


$(BUILD_DIR)/$(EXE) : $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) 

$(BUILD_DIR)/main.o : src/main.c src/fred.h src/common.h | $(BUILD_DIR)
	$(CC) -c -o $@ $< $(CFLAGS)

$(BUILD_DIR)/fred.o : src/fred.c src/fred.h src/common.h | $(BUILD_DIR)
	$(CC) -c -o $@ $< $(CFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)




$(DEBUG_DIR)/$(EXE): $(DEBUG_OBJS)
	$(CC) -g -o $@ $^ $(CFLAGS) 

$(DEBUG_DIR)/main.o : src/main.c src/fred.h src/common.h | $(DEBUG_DIR)
	$(CC) -g -c -o $@ $< $(CFLAGS)

$(DEBUG_DIR)/fred.o : src/fred.c src/fred.h src/common.h | $(DEBUG_DIR)
	$(CC) -g -c -o $@ $< $(CFLAGS)

$(DEBUG_DIR): 
	mkdir $(DEBUG_DIR)


$(TEST_DIR)/test : $(TEST_DIR)/test.o $(BUILD_DIR)/fred.o
	$(CC) -o $@ $^ $(CFLAGS) 

$(TEST_DIR)/test.o : $(TEST_DIR)/test.c
	$(CC) -c -o $@ $? $(CFLAGS) 

clean:
	rm -r $(BUILD_DIR)/*

clean_debug:
	rm -r $(DEBUG_DIR)/*




# https://stackoverflow.com/questions/5178125/how-to-place-object-files-in-separate-subdirectory

# debugging with gdb:  https://stackoverflow.com/questions/8963208/gdb-display-output-of-target-application-in-a-separate-window
# debuggin with gdbserver: https://stackoverflow.com/a/15306382 (prefer this, for some reason using redirecting with tty 
# causes the output terminal to crash sometimes)
#
