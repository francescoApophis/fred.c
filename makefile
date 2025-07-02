
EXE = fred
CC = gcc 
CFLAGS = -Wall -Wextra -Wpedantic -Wno-comment
BUILD_DIR = ./build
DEBUG_DIR = ./debug
TEST_DIR = ./tests

OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/fred.o
DEBUG_OBJS = $(DEBUG_DIR)/main.o  $(DEBUG_DIR)/fred.o

.PHONY = all                \
				 Debug 							\
				 Test 							\
         clean              \
         clean_debug        \
         $(DEBUG_DIR)/fred  \
				 $(TEST_DIR)/test 

all: $(BUILD_DIR)/$(EXE)

Debug: $(DEBUG_DIR)/$(EXE)

Test: $(TEST_DIR)/test 


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
	mkdir -p $(DEBUG_DIR)


$(TEST_DIR)/test : $(TEST_DIR)/test.h $(TEST_DIR)/test.c src/common.h src/fred.h src/fred.c
	$(CC) -g -o $@ $^ $(CFLAGS) 





clean:
	rm -r $(BUILD_DIR)/*

clean_debug:
	rm -r $(DEBUG_DIR)/*




