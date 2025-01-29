

EXE = fred
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic
BDIR = ./build
OBJS = $(BDIR)/main.o

.PHONY = all clean

all: $(BDIR)/$(EXE) 

$(BDIR)/$(EXE) : $(OBJS) 
	$(CC) -o $@ $< $(CFLAGS) 

$(BDIR)/%.o : src/%.c src/fred.h
	mkdir -p $(BDIR)
	$(CC) -c -o $@ $< $(CFLAGS)

clean :
	$(RM) $(BDIR)/*

# https://stackoverflow.com/questions/5178125/how-to-place-object-files-in-separate-subdirectory

