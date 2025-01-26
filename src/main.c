#include <errno.h>
#include <termios.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "fred.h"


#define GOTO_END(value) do { failed = (value) ; goto end; } while (0)

// TODO: about the 'FAILURE: message printing, I could pass it to goto_end'
#define ERROR(msg) do { \
  fprintf(stderr, "Error [in: %s, at line: %d]: %s (errno=%d)\n", \
                  __FILE__, __LINE__, msg, errno); \
  GOTO_END(1); \
} while (0)



bool FRED_open_file(FredFile* ff, const char* file_path)
{
  bool failed = 0;   
  bool loaded_file = 0;

  struct stat sb;
  if (stat(file_path, &sb) == -1) {
    if (errno == ENOENT) ERROR("no such file");
    else ERROR("could not retrieve any information about given file.");
  }

  if ((sb.st_mode & S_IFMT) != S_IFREG) {
    if ((sb.st_mode & S_IFMT) == S_IFDIR) {
      ERROR("given path is directory. Please provide a file-path.");
    } 
    ERROR("given file is not a regular file.");
  }

  int fd = open(file_path, O_RDONLY);
  if (fd == -1) ERROR("could not open given file.");

  ff->size = sb.st_size;
  ff->text = malloc(sizeof(*ff->text) * ff->size);
  if (ff->text == NULL) ERROR("not enough memory for file buffer.");
  loaded_file = 1;

  ssize_t bytes_read = read(fd, ff->text, ff->size);
  if (bytes_read == -1) ERROR("could not read file content.");

  while ((size_t) bytes_read < ff->size) {
    ssize_t new_bytes_read = read(fd, ff->text + bytes_read, ff->size - bytes_read);
    if (new_bytes_read == -1) {
      ERROR("could not read file content.");
    }
    bytes_read += new_bytes_read;
  }

  if (close(fd) == -1) {
    ERROR("could not close given file.");
  }
  
  GOTO_END(failed);

end:
  if (loaded_file && failed) free(ff->text);
  return failed; 
}


bool FRED_setup_terminal(termios* term_orig)
{
  bool failed = 0;

  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)){
    ERROR("Fred editor can only be run on a terminal.");
  }
  
  if (tcgetattr(STDIN_FILENO, term_orig) == -1){
    ERROR("could not retrieve terminal options.");
  }

  termios term_raw = *term_orig;
  term_raw.c_iflag &= ~(BRKINT | ISTRIP | INPCK |  IXON); // ICRNL |
  term_raw.c_oflag &= ~OPOST;
  term_raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  term_raw.c_cflag &= ~(CSIZE | PARENB);
  term_raw.c_cflag |= CS8;
  term_raw.c_cc[VTIME] = 5;
  term_raw.c_cc[VMIN] = 0;

  if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_raw)){
    ERROR("could not set terminal options.");
  }
  GOTO_END(failed);

end:
  return failed;
}


bool fred_editor_init(FredEditor* fe, FredFile* ff)
{
  bool failed = 0;

  PIECE_TABLE_INIT(&fe->pt);
  PIECE_TABLE_PUSH(&fe->pt, ((Piece){
    .which_buf = 0,
    .offset = 250,
    .len = ff->size,
  }));
  fe->cursor = ((Cursor){.row = 0, .col = 0});

  GOTO_END(failed);
end:
  return failed;
}


bool FRED_render(bool idle, FredFile* ff)
{
  bool failed = 0;

  GOTO_END(failed);
end:
  return failed;
}

bool FRED_start_editor(FredFile* ff)
{
  bool failed = 0;
  FredEditor fe;
  fred_editor_init(&fe, ff);

  bool running = true;
  bool idle = false;
  char key;
  while (running) {
    ssize_t b_read = read(STDIN_FILENO, &key, 1);
    if (b_read == -1) ERROR("couldn't read from stdin");

    idle = (bool)b_read;
    if (key == 'q') running = false;
    failed = FRED_render(idle, ff);
    if (failed) GOTO_END(failed);
    key = 0;
  }

  GOTO_END(failed);

end:
  PIECE_TABLE_FREE(&fe.pt);
  return failed;
}



int main(int argc, char** argv)
{
  // TODO: move ff into a editor struct?
  // REMEMBER: JUST MAKE SOMETHING THAT WORKS FIRST!!!!!!!!
  bool failed = 0;
  bool term_set = 0;

  if (argc < 2) ERROR("no file-path provided.");
  if (argc > 2) ERROR("too many arguments; can only handle one file right now.");

  char* filepath = argv[1];
  FredFile ff;
  failed = FRED_open_file(&ff, filepath);
  if (failed) GOTO_END(1);

  termios term_orig;
  failed = FRED_setup_terminal(&term_orig);
  if (failed) GOTO_END(1);
  else term_set = 1;

  failed = FRED_start_editor(&ff);
  if (failed) GOTO_END(1);

  GOTO_END(failed);

end:
  if (term_set) {
    while (8 != write(STDOUT_FILENO, "\033[2J\033[H", 8)){}
    // TODO: not really sure if this is the best way to handle it.
    // What if something causes tcsetattr() to fail continously?
    while (0 != tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_orig)){}

    free(ff.text);
  }
  return failed;
}

// For the error-handling with goto's: 
// https://github.com/tsoding/noed
// Copyright 2022 Alexey Kutepov <reximkut@gmail.com>

// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:

// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
