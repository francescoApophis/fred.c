exe = fred
files = src/fred.h \
			 	src/main.c \

.PHONY: build debug

build : $(files) 
	mkdir -p ./build/
	gcc $(files) -o ./build/$(exe) -Wall -Wextra -Wpedantic

debug : $(files)
	mkdir -p ./build/
	gcc $(files) -g -o  ./build/$(exe)_debug -Wall -Wextra -Wpedantic
	gdb --args ./build/$(exe)_debug src/main.c

clean : 
	rm ./build/$(exe)
