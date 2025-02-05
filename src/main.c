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


void printing(void)
{
  tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
  sigaction(SIGWINCH, &old, NULL);
  fprintf(stdout, "\033[2J\033[H");
}

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


bool FRED_save_file(FredEditor* fe, const char* file_path)
{
  bool failed = 0;
  size_t file_size = 0;

  for (size_t i = 0; i < fe->piece_table.len; i++){
    file_size += fe->piece_table.items[i].len;
  }

  FILE* f = fopen(file_path, "wb");
  if (f == NULL){
    ERROR("could not save file: failed to open file.");
  }

  if (file_size > 0){
    for (size_t j = 0; j < fe->piece_table.len; j++){
      Piece* piece = fe->piece_table.items + j;
      char* buf = !piece->which_buf ? fe->file_buf.text : fe->add_buf.items;
      fwrite(buf + piece->offset, sizeof(*buf), piece->len, f);
    }
  } else {
    int fd = fileno(f);
    ftruncate(fd, 0);
  }

  fclose(f);
end: 
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

  fe->cursor = (Cursor){0};
  fe->line_num_w = 8;

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

void fred_editor_free(FredEditor* fe)
{
  DA_FREE(&fe->piece_table, true);
  DA_FREE(&fe->add_buf, true);
  free(fe->file_buf.text);
}


bool fred_win_resize(TermWin* tw)
{
  bool failed = false;
  struct winsize w;
  if (-1 == ioctl(STDIN_FILENO, TIOCGWINSZ, &w)){
    ERROR("could not retrieve terminal size.");
  }
  
  tw->size = w.ws_row * w.ws_col;
  tw->rows = w.ws_row;
  tw->cols = w.ws_col;
  void* temp = realloc(tw->text, tw->size);
  if (temp == NULL) ERROR("not enough space to get and display text.");
  tw->text = temp;
  GOTO_END(failed);
end:
  return failed;
}



void FRED_move_cursor(FredEditor* fe, TermWin* tw, char key)
{
  // TODO: maybe binary search the piece-table
  size_t tot_rows = 0;
  size_t line_len_at_curs = 0;
  size_t line_len_at_curs_up = 0;
  size_t line_len_at_curs_down = 0;
  Piece* piece = fe->piece_table.items;
  Piece* last_piece = fe->piece_table.items + fe->piece_table.len;
  char* buf = !piece->which_buf ? fe->file_buf.text : fe->add_buf.items;

  for (size_t i = 0; i < piece->len; i++){
    if (buf[i] == '\n') {
      tot_rows = ((tot_rows + 1) * tw->cols) / tw->cols;
    }else {
      if (tot_rows == fe->cursor.row){
        line_len_at_curs++;
      } else if (tot_rows == fe->cursor.row - 1){
        line_len_at_curs_up++;
      } else if (tot_rows == fe->cursor.row + 1){
        line_len_at_curs_down++;
      }
    }

    if (i + 1 >= piece->len){
      if (piece + 1 >= last_piece) break;
      piece++;
      buf = !piece->which_buf ? fe->file_buf.text : fe->add_buf.items;
    }
  }
  
  if (line_len_at_curs_up > 0) line_len_at_curs_up--;
  if (line_len_at_curs > 0) line_len_at_curs--;
  if (line_len_at_curs_down > 0) line_len_at_curs_down--;
  
  switch (key){
    case 'j': {
      if (fe->cursor.row + 1 >= tot_rows) return; 
      if (fe->cursor.win_row + 1 >= tw->rows - 1) return;
      
      fe->cursor.row++;
      if (fe->cursor.win_row + 1 < tw->rows - tw->rows / 3 || fe->cursor.row + 1 > tot_rows - tw->rows / 3){
        fe->cursor.win_row++;
      } else {
        tw->lines_to_scroll++; 
      }

      if (fe->cursor.col >= line_len_at_curs_down){
        fe->cursor.col = line_len_at_curs_down;
        fe->cursor.win_col = line_len_at_curs_down;
      }
      return;
    }
    case 'k': {
      if ((int)fe->cursor.row - 1 < 0) return;
      if ((int)fe->cursor.win_row - 1 < 0) return;

      fe->cursor.row--;
      if (fe->cursor.win_row - 1 < tw->rows / 3 && (int)tw->lines_to_scroll > 0) {
        tw->lines_to_scroll--;
      } else {
        fe->cursor.win_row--;
      }

      if (fe->cursor.col >= line_len_at_curs_up){
        fe->cursor.col = line_len_at_curs_up;
        fe->cursor.win_col = line_len_at_curs_up;
      }
      return;
    }

    case 'h': {
      if (fe->cursor.col == 0) return;
      fe->cursor.col--; 
      fe->cursor.win_col--;
      return;
    }
    case 'l': {
      if (fe->cursor.col + 1 <= line_len_at_curs){
        fe->cursor.col++;
        fe->cursor.win_col++;
      }
      return;
    }
  }
}


void fred_get_text_from_piece_table(FredEditor* fe, TermWin* tw, bool insert)
{
  memset(tw->text, SPACE_CH, tw->size);

  char* mode = insert ? "-- INSERT --" : "-- NORMAL --";
  size_t last_row_idx = (tw->rows - 1) * tw->cols + 1;
  memcpy(tw->text + last_row_idx, mode, strlen(mode));
  
  // display cursor row:col
  sprintf(tw->text + last_row_idx + tw->cols - 10, 
          "%d:%d", (int)fe->cursor.row + 1, (int)fe->cursor.col + 1);

  size_t tw_text_idx = fe->line_num_w;
  size_t lines_to_scroll = tw->lines_to_scroll;
  Piece* piece = fe->piece_table.items;
  Piece* last_piece = fe->piece_table.items + fe->piece_table.len;
  char* buf = !piece->which_buf ? fe->file_buf.text : fe->add_buf.items;
  size_t tot_lines = 0;
  size_t line_wrapped_rows = 0;

  for (size_t i = 0; i < piece->len; i++){
    size_t buf_idx = piece->offset + i;

    if (lines_to_scroll > 0) {
      if (buf[buf_idx] == '\n') lines_to_scroll--;
      continue;
    }

    size_t tw_row = tw_text_idx / tw->cols;
    size_t tw_col = tw_text_idx % tw->cols;

    if (tw_text_idx >= tw->size) break;
    if (tw_row >= tw->rows - 1) break;

    if (buf[buf_idx] != '\n'){
      if (tw_col == 0){
        tw_text_idx += fe->line_num_w;
        line_wrapped_rows++;
      }
      tw->text[tw_text_idx++] = buf[buf_idx];

    } else {
      int line_scrolled = tot_lines + tw->lines_to_scroll + 1;
      char line_digits = snprintf(NULL, 0, "%d", line_scrolled);
      char line_str[line_digits]; 
      sprintf(line_str, "%d", line_scrolled);
      memcpy(tw->text + LINE_NUM_OFFSET, line_str, line_digits);
      line_wrapped_rows = 0;
      tot_lines++;
      tw_text_idx = (tw_row + 1) * tw->cols + fe->line_num_w;
    }

    if (buf_idx + 1 >= piece->len){
      if (piece + 1 >= last_piece) break;
      piece++;
      buf = !piece->which_buf ? 
        fe->file_buf.text : fe->add_buf.items;
    } 
  }
}

bool FRED_render_text(TermWin* tw, Cursor* cursor, short line_num_w)
{
  bool failed = 0;
  fprintf(stdout, "\x1b[H");
  fwrite(tw->text, sizeof(*tw->text), tw->size, stdout);
  fprintf(stdout, "\033[%zu;%zuH", 
          cursor->win_row + 1, 
          line_num_w + cursor->win_col + 1); // TODO: use win_row, win_col
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

  ADD_BUF_PUSH(&fe->add_buf, key);

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


bool FRED_start_editor(FredEditor* fe, const char* file_path)
{
  bool failed = 0;
  bool running = true;
  bool insert = false;
  char key;

  TermWin tw = {0};
  fred_win_resize(&tw);

  while (running) {
    fred_get_text_from_piece_table(fe, &tw, insert);
    FRED_render_text(&tw, &fe->cursor, fe->line_num_w);

    ssize_t b_read = read(STDIN_FILENO, &key, 10);
    if (b_read == -1) {
      if (errno == EINTR){
        fred_win_resize(&tw);
        continue;
      }
      ERROR("couldn't read from stdin");
    }

    if (insert) {
      if (key == ESC_CH) insert = false;

      if (key == '\n' || (key >= ' ' && key <= '~')){
        fred_make_piece(fe, key);
        // TODO: this should not be handled here
        fe->cursor.col++;
        fe->cursor.win_col++;
      }  

      if (key == '\n') {
        fe->cursor.row++;
        fe->cursor.col = 0;
        fe->cursor.win_col = 0;
      }
    } else {
      switch(key) {
        case 'w': { FRED_save_file(fe, file_path); break;}
        case 'q': { running = false; break;}
        case 'i': { insert = true; break;}

        case 'h': 
        case 'j': 
        case 'k': 
        case 'l': { FRED_move_cursor(fe, &tw, key); break; } 
        case ESC_CH: { insert = false; break; }
        default:{}
      }
    }
    key = 0;
  }

  GOTO_END(failed);

end:
  fred_editor_free(fe);
  free(tw.text);
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
  failed = fred_editor_init(&fe, file_path);
  if (failed) GOTO_END(1);

  failed = FRED_start_editor(&fe, file_path);
  if (failed) GOTO_END(1);


  GOTO_END(failed);
end:
  return failed;
}

// at the moment fred is able to render text and write to an empty file.
// You cannot navigate end edit just write, almost as in unbuffered mode.
// GOAL: 
//  - edit at cursor pos.
//  - squash pieces into signle pieces. 
//  - delete characters

// FIXME: when i zoom in the terminal to size 27*112, if the 
// first line is wrapped, its line-num is rendered in the wrong spot

// TODO: fred_get_text_from_piece_table() is doing unrelated things either
// call it get_all_text or move something out

// TODO: add testing

// TODO: add a line-limit 

// TODO: if any of the realloc's (win_resize, DA_macros) were to fail,
// since the contents should not be touched, I think the user 
// should be prompted whether or not to write the all the edits 
// to the file.

// TODO: handle ftruncate error

// TODO: differentiate errors for the user and internal errors. Use strerror()
// TODO: have a separate buffer to report error messages in the 
// bottom line of the screen
// TODO: if i choose to do the above, change fred_get_text_from_piece_table()
// to fred_get_all_text()

// TODO: handle eintr on close() in open_file()

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

// TODO: cursor should not move outside text in line.


// TODO: when entering insert mode, push a SINGLE PIECE
// and increase it's length as you type. 
// But the thing is that when entering insert mode, 2 actions
// can be made: inserting new text or deleting old text.
//
// So when entering insert mode, I cannot just push a piece 
// directly, instead I should: 
//   - parse and reach the 'right' index in the piece table where 
//   the edit should happen;
//
//   There's two cases when reaching the the 'right' piece index:
//       1. the cursor position is 'inside' a piece (cursor is for example
//       at (row = 0, col = 15) and the piece as (offset = 0, len = 52))
//
//       2. the cursor position is 'after' a piece.
//
//    So to reach the right piece_index, i will loop through the table and 
//    check if the length is less the cursor coordinates.
//
//   - then, based on the key-press (BACKSPACE/any text-key) i should 
//   edit: decrease the piece length if deleting or pushing a new piece
//   and increasing it's length if inserting.
//       
// If you delete decrease the length; 
// if the piece has length==0 ? 
//  if the piece.idx == piece_table.len ? decrease piece_table.len
//  else do the memmove thingy
//

// TODO: in the future if i decide to add custom key-maps, I could 
// use an array with designated initializers to hold mappings
// mappings = { [ESC_KEY] = void (*some_job)(void*)...,
             // [SPACE_KEY] = void (*some_job)(void*)...,}
// where ESC_KEY is a preproc constant


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
