#include <errno.h>
#include <sys/ioctl.h>
#include <signal.h>
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


termios term_orig = {0};
struct sigaction act = {0};
struct sigaction old = {0};


bool FRED_open_file(FileBuf* file_buf, const char* file_path)
{
  bool failed = 0;   
  bool file_loaded = 0;

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

  file_buf->size = sb.st_size;
  file_buf->text = malloc(sizeof(*file_buf->text) * file_buf->size);
  if (file_buf->text == NULL) ERROR("not enough memory for file bufile_bufer.");
  file_loaded = 1;

  ssize_t bytes_read = read(fd, file_buf->text, file_buf->size);
  if (bytes_read == -1) ERROR("could not read file content.");

  while ((size_t) bytes_read < file_buf->size) {
    ssize_t new_bytes_read = read(fd, file_buf->text + bytes_read, file_buf->size - bytes_read);
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
  if (file_loaded && failed) free(file_buf->text);
  return failed; 
}


void handle_win_resize_sig(int sig)
{
  (void)sig;
}


bool FRED_setup_terminal()
{
  bool failed = 0;
  bool term_set = 0;

  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)){
    ERROR("Fred editor can only be run on a terminal.");
  }
  
  if (tcgetattr(STDIN_FILENO, &term_orig) == -1){
    ERROR("could not retrieve terminal opiece_tableions.");
  }

  termios term_raw = term_orig;
  term_raw.c_iflag &= ~(BRKINT | ISTRIP | INPCK |  IXON); // ICRNL |
  term_raw.c_lflag &= ~(ICANON | IEXTEN);
  term_raw.c_cflag &= ~(CSIZE | PARENB);
  term_raw.c_cflag |= CS8;
  term_raw.c_cc[VTIME] = 5;
  term_raw.c_cc[VMIN] = 0;

  if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_raw)){
    ERROR("could not set terminal opiece_tableions.");
  }

  act.sa_handler = handle_win_resize_sig;
  if (-1 == sigaction(SIGWINCH, &act, &old)){
    ERROR("could not set the editor to detect window changes.");
  }

  GOTO_END(failed);
end:
  if (failed && term_set) {
    tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
  }
  return failed;
}


bool fred_editor_init(FredEditor* fe, const char* file_path)
{
  bool failed = 0;
  bool file_loaded = 0;
  bool term_and_sig_set = 0;
  bool piece_table_allocated = 0;

  DA_INIT(&fe->piece_table);
  DA_INIT(&fe->add_buf);
  failed = FRED_open_file(&fe->file_buf, file_path);
  if (failed) GOTO_END(1);
  file_loaded = 1;

  failed = FRED_setup_terminal(&term_orig);
  if (failed) GOTO_END(1);
  term_and_sig_set = 1;

  if (fe->file_buf.size > 0){
    PIECE_TABLE_PUSH(&fe->piece_table, ((Piece){
      .which_buf = 0,
      .offset = 0,
      .len = fe->file_buf.size,
    }));
    piece_table_allocated = 1;
  }

  fe->cursor = ((Cursor){.row = 0, .col = 0});

  GOTO_END(failed);
end:
  if (failed){
    if (file_loaded) free(fe->file_buf.text);

    if (term_and_sig_set) {
      tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
      sigaction(SIGWINCH, &old, NULL);
    }

    if (piece_table_allocated){
      DA_FREE(&fe->piece_table, true);
    }
  }
  return failed;
}


bool fred_win_resize(Display* d)
{
  bool failed = false;
  struct winsize w;
  if (-1 == ioctl(STDIN_FILENO, TIOCGWINSZ, &w)){
    ERROR("could not retrieve terminal size.");
  }

  d->size = w.ws_row * w.ws_col;
  d->rows = w.ws_row;
  d->cols = w.ws_col;
  void* temp = realloc(d->text, d->size);
  if (temp == NULL) ERROR("not enough space to get and display text.");
  d->text = temp;
  GOTO_END(failed);
end:
  return failed;
}


void fred_grab_text(FredEditor* fe, Display* d, size_t scroll)
{
  (void)scroll;

  for (size_t i = 0; i < d->size; i++){
    d->text[i] = ' ';
  }

  size_t i = 0;
  size_t p_idx = 0;

  while (i < d->size && p_idx < fe->piece_table.len){
    Piece* piece = fe->piece_table.items + p_idx;
    char* buf = !piece->which_buf ? fe->file_buf.text : fe->add_buf.items;
    
    for (size_t j = 0; j < piece->len; j++){
      if (i >= d->size) break; // NOTE: it crashes if the first piece is the 'original' file-buf piece
                               // and the file-buf is a big ass file of size is bigger than display
      size_t row = i / d->cols;
      if (buf[piece->offset + j] != '\n'){
        d->text[i++] = buf[piece->offset + j];
      } else {
        i = (row + 1) * d->cols;
      }
    }
    p_idx++;
  }
}

bool FRED_render_text(Display* d, Cursor* c)
{
  
  bool failed = 0;
  fprintf(stdout, "\x1b[H");
  fwrite(d->text, sizeof(*d->text), d->size, stdout);
  fprintf(stdout, "\033[%zu;%zuH", c->row + 1, c->col + 1);
  fflush(stdout);
  GOTO_END(failed);
end:
  return failed;
}



bool fred_make_piece(FredEditor* fe, char key)
{
  bool failed = 0;
  
  
  size_t offset = 0;
  if (fe->piece_table.len > 0) {
    Piece last_piece = fe->piece_table.items[fe->piece_table.len - 1];
    offset = last_piece.offset + last_piece.len;
  }

  PIECE_TABLE_PUSH(&fe->piece_table, ((Piece){
    .which_buf = 1, 
    .offset = offset, 
    .len = 1,
  }));

  DA_PUSH(&fe->add_buf, key, ADD_BUF_INIT_CAP, "add_buf");

  GOTO_END(failed);
end:
  return failed;
}


void dump_piece_table(FredEditor* fe)
{
  for (size_t i = 0; i < fe->piece_table.len; i++){
    Piece piece = fe->piece_table.items[i];
    char buf = !piece.which_buf ? 
                      fe->file_buf.text[piece.offset] : 
                      fe->add_buf.items[piece.offset] ;
    fprintf(stdout, "piece at [%ld] = {buf = %d, offset = %lu, len = %lu} \"%c\"\n", 
            i, piece.which_buf, piece.offset, piece.len, buf);
  }
}


bool FRED_start_editor(FredEditor* fe)
{
  bool failed = 0;
  bool running = true;
  char key;
  size_t scroll = 0;

  Display d = {0};
  fred_win_resize(&d);

  while (running) {
    fred_grab_text(fe, &d, scroll);
    FRED_render_text(&d, &fe->cursor);

    ssize_t b_read = read(STDIN_FILENO, &key, 10);
    if (b_read == -1) {
      if (errno == EINTR){
        fred_win_resize(&d);
        continue;
      }
      ERROR("couldn't read from stdin");
    }

    switch(key) {
      case 'q': { running = false; break;}
      case 27 : {
        size_t i = 0;
        size_t file_final_size = 0;
        while (i++ < fe->piece_table.len){
          file_final_size += fe->piece_table.items[i].len;
        }

        if (file_final_size > 0){
          FILE* f = fopen(fe->file_path, "wb");

          size_t j = 0;
          while (j < fe->piece_table.len){
            Piece* piece = fe->piece_table.items + j;
            char* buf = !piece->which_buf ? fe->file_buf.text : fe->add_buf.items;
            fwrite(buf + piece->offset, sizeof(*buf), piece->len, f);
            j++;
          }
        }
        break;
      }
      default : {
        if (key == '\n' || (key >= ' ' && key <= '~')){
          fred_make_piece(fe, key);
          fe->cursor.col++;
        }  

        if (key == '\n') {
          fe->cursor.row++;
          fe->cursor.col = 0;
        }
      }
    }
    key = 0;
  }

  GOTO_END(failed);

end:
  DA_FREE(&fe->piece_table, true);
  DA_FREE(&fe->add_buf, true);
  free(fe->file_buf.text);
  tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
  sigaction(SIGWINCH, &old, NULL);
  fprintf(stdout, "\033[2J\033[H");

  return failed;
}



int main(int argc, char** argv)
{
  // REMEMBER: JUST MAKE SOMETHING THAT WORKS FIRST!!!!!!!!
  bool failed = 0;

  if (argc < 2) ERROR("no file-path provided.");
  if (argc > 2) ERROR("too many arguments; can only handle one file right now.");

  char* file_path = argv[1];

  FredEditor fe = {0};
  fe.file_path = file_path;
  failed = fred_editor_init(&fe, file_path);
  if (failed) GOTO_END(1);

  failed = FRED_start_editor(&fe);
  if (failed) GOTO_END(1);


  GOTO_END(failed);
end:
  return failed;
}



// TODO: grab_text() -> get_text() wtf was i even thinking 
 
// TODO: handle eintr on close() in open_file()

// TODO: display -> screen

// TODO: handle failures in render_text()

// TODO: I probably need a flag to differentiate action 
// such as writing, deleting etc in make_piece()

// TODO: what if I save each '\n' as a separate piece?
// it would make the text-retrieval for rendering much easier 
// (i could just while-loop through d->text and step through the 
// piece-table with a variable; i could just memcpy each text-piece 
// without having to check for newlines).
// but idk maybe it's a waste of space.

// TODO: use cursor position with offset in make_piece()

// TODO: don't use hardcoded which_buf in make_piece()

// TODO: make macro for add_buf push in make_piece()

// TODO: I could have a fixed-size buffer something
// not too big (100 chars) where character typed are stored. From 
// this buffer render the char on the screen. 
// Once you exit inser mode (or even save maybe?) push 
// the make a piece table out of the buffer and clear
// the latter.





// For the error-handling: 
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
