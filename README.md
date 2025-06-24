# fred.c
Vim-like Terminal Text-Editor in C.

# Dependencies 
- C-compiler 
- GNU-make
The editor can only run on a Linux operating system 
(tho I only tried it on Ubuntu so I don't really know).

For test *generation*:
- Neovim v0.9.5 (or later I guess)
- Linux operating system for commands


# Quick Installation
- ```git clone``` the repo
- run ```$ make```
- run ```$ ./build/fred <filename>```

# Commands
There are two modes: 
- Normal: navigate through the file
- Insert: edit the file

(Work in progress)

| Key | Desc |
| ----------- | -------- |
| ```i``` | Switch to Insert mode from Normal mode | 
| ```Esc``` | Switch to Normal mode from Insert mode |
| ```k``` | Move up |
| ```j``` | Move down |
| ```h``` | Move left |
| ```l``` | Move right |
| ```q``` | Quit |
| ```Backspace``` | Delete text |

# Debugging 
For debugging: 
- ```$ make Debug```
[You can then follow these instruction to debug with gdb](https://stackoverflow.com/a/15306382)
from the terminal, which is what I use since I don't really know about 
other debuggers.

# Testing 
Fred uses a [piece-table](https://en.wikipedia.org/wiki/Piece_table)
to store and edit text, and ```test.c``` at the moment tests only that
part of the editor.

A ```fred_test``` feeds random keys and confronts the 
Fred's output with the key's respective **snapshot**, the 
expected output. On failure it reports info with highlighted 
differences between the outputs.

Read more about the ```fred_test``` folder content in 
```gen_test_with_neovim.lu```

If you want to generate new ```fred_test``` folders use the 
```gen_test_with_neovim.lua``` script, which relies on 
Neovim v0.9.5 or later and Linux commands.


To run a test: 
- ```$ make Test```
- ```$ ./tests/test ./test/fred_test```


# Special thanks:

Thanks for for  the main loop structure in FRED_start_editor(), 
the idea in FRED_get_text_to_render() of storing 
the to-be-rendered text in a buffer, the error handling 
and more generally for the inspiration at the beginning of the project:
https://github.com/tsoding/noed

