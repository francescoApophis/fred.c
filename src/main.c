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
  // sigaction(SIGWINCH, &old, NULL);
  fprintf(stdout, "\033[2J\033[H");
}

#define PRINTING(statement) do { \
  printing(); \
  { \
    statement \
  } \
  exit(1); \
} while (0)



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
  term_raw.c_lflag &= ~(ICANON | IEXTEN | ECHO);
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
    // PIECE_TABLE_PUSH(&fe->piece_table, ((Piece){
      // .which_buf = 0,
      // .offset = 0,
      // .len = fe->file_buf.size,
    // }));
    // piece_table_allocated = 1;

    int d = 10;
    PIECE_TABLE_PUSH(&fe->piece_table, ((Piece){
      .which_buf = 0,
      .offset = 0,
      .len = d,
    }));

    PIECE_TABLE_PUSH(&fe->piece_table, ((Piece){
      .which_buf = 0,
      .offset = d, 
      .len = fe->file_buf.size - d,
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


bool FRED_win_resize(TermWin* tw)
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
  size_t tot_lines = 0;
  size_t curs_line_len = 0;
  size_t curs_up_line_len = 0;
  size_t curs_down_line_len = 0;

  size_t last_newline_idx = 0; // 'idx' in the text constructed from the piece-table
  size_t tot_text_len = 0;
  size_t tw_text_row_width = tw->cols - fe->line_num_w;

  size_t tot_rows = 0;
  size_t tw_lines_rows_idx = 0;
  int tw_lines_rows[tw->rows - 1];
  memset(&tw_lines_rows, -1, tw->rows - 1);

  for (size_t piece_idx = 0; piece_idx < fe->piece_table.len; piece_idx++){
    Piece* piece = fe->piece_table.items + piece_idx;
    char* buf = !piece->which_buf ? fe->file_buf.text : fe->add_buf.items;

    for (size_t i = 0; i < piece->len; i++){
      if (buf[piece->offset + i] != '\n') continue;
      
      size_t newline_idx = tot_text_len + i; // NOTE+TODO: what if the newline char is at i=0?
      size_t line_len = newline_idx - last_newline_idx + (last_newline_idx? -1 : 0);
      last_newline_idx = newline_idx;

      if (tot_lines >= tw->lines_to_scroll && tot_rows < tw->rows - 1){
        size_t line_rows = (line_len + tw_text_row_width - line_len % (tw_text_row_width)) / tw_text_row_width; // wrapped rows
        tot_rows += line_rows;
        tw_lines_rows[tw_lines_rows_idx++] = line_rows;
      }

      if (tot_lines == fe->cursor.row)          curs_line_len = line_len;
      else if (tot_lines == fe->cursor.row - 1) curs_up_line_len = line_len;
      else if (tot_lines == fe->cursor.row + 1) curs_down_line_len = line_len;

      tot_lines++;
    }
    tot_text_len += piece->len;
  }

  // NOTE: cursor.win_row/win_col is repositioned in fred_grab_text_from_piece_table()
  // because it's easier doing it there when the window gets resized/zoomed 
  switch (key){
    case 'j': {
      if (fe->cursor.row + 1 >= tot_lines) return; 

      fe->cursor.row++;
      if (fe->cursor.col >= curs_down_line_len){
        fe->cursor.col = curs_down_line_len - (curs_down_line_len ? 1 : 0);
      }

      size_t curs_line_rows = (curs_line_len + tw_text_row_width - curs_line_len % (tw_text_row_width)) / tw_text_row_width;

      if (fe->cursor.win_row + curs_line_rows >= tw->rows / 2 + 5){
        if ((int)curs_line_rows <= tw_lines_rows[0]){
          ++tw->lines_to_scroll;
          return;
        } 
        // NOTE: scroll multiple lines if the next line is too long to render properly
        for (size_t i = 0, rows_to_scroll = 0; i < tw_lines_rows_idx + 1; i++){
          if (rows_to_scroll > curs_line_rows) return;
          rows_to_scroll += tw_lines_rows[i];
          ++tw->lines_to_scroll;
        }
      }
      return;
    }
    
    case 'k': {
      if ((int)fe->cursor.row - 1 < 0) return;

      size_t curs_up_line_rows = (curs_up_line_len + tw_text_row_width - 
                                  curs_up_line_len % (tw_text_row_width)) / tw_text_row_width; 

      if (fe->cursor.col >= curs_up_line_len){
        fe->cursor.col = curs_up_line_len - (curs_up_line_len ? 1 : 0);
      }
      fe->cursor.row--;
      if (fe->cursor.win_row - curs_up_line_rows < tw->rows / 2 - 5 || fe->cursor.row <= tw->rows / 2 - 5){
        if (tw->lines_to_scroll) tw->lines_to_scroll--;
      }
      return;
    }

    case 'h': {
      if ((int)fe->cursor.col - 1 < 0) return;
      fe->cursor.col--;
      return;
    }

    case 'l': {
      if (fe->cursor.col + 1 >= curs_line_len) return;
      fe->cursor.col++;
      return;
    }
  }
}


void FRED_get_text_to_render(FredEditor* fe, TermWin* tw, bool insert)
{
  memset(tw->text, SPACE_CH, tw->size);

  char* mode = insert ? "-- INSERT --" : "-- NORMAL --";
  size_t last_row_idx = (tw->rows - 1) * tw->cols + 1;
  memcpy(tw->text + last_row_idx, mode, strlen(mode));
  sprintf(tw->text + last_row_idx + tw->cols - 10, "%d:%d", (int)fe->cursor.row + 1, (int)fe->cursor.col + 1);

  size_t tw_text_idx = fe->line_num_w;
  size_t tot_lines = 0;
  int tot_text_len = 0;
  int last_newline_idx = 0;

  for (size_t piece_idx = 0; piece_idx < fe->piece_table.len; piece_idx++){
    Piece* piece = fe->piece_table.items + piece_idx;
    char* buf = !piece->which_buf ? fe->file_buf.text : fe->add_buf.items;

    for (size_t i = 0; i < piece->len; i++){
      size_t buf_idx = piece->offset + i;
      size_t tw_row = tw_text_idx / tw->cols;
      size_t tw_col = tw_text_idx % tw->cols;

      if (tw_row >= tw->rows - 1) return;

      if (tot_lines < tw->lines_to_scroll){
        if (buf[buf_idx] == '\n') tot_lines++;
        continue;
      }

      tot_text_len++;
      if (tot_text_len - last_newline_idx == 1){
        int n = tot_lines + 1;
        char line_num_digits = snprintf(NULL, 0, "%d", n);
        char line_num[line_num_digits]; 
        sprintf(line_num, "%d", n);
        int offset = tw_text_idx - fe->line_num_w / 3 - line_num_digits;
        memcpy(tw->text + offset, line_num, line_num_digits);

        if (tot_lines == fe->cursor.row){
          size_t tw_text_row_w = tw->cols - fe->line_num_w;
          fe->cursor.win_col = fe->cursor.col % tw_text_row_w;
          fe->cursor.win_row = tw_row + (fe->cursor.col - fe->cursor.col % tw_text_row_w) / tw_text_row_w;
        }
      }

      if (buf[buf_idx] != '\n'){
        if (tw_col == 0) tw_text_idx += fe->line_num_w;
        tw->text[tw_text_idx++] = buf[buf_idx];
        continue;
      } 

      last_newline_idx = tot_text_len;
      tot_lines++;
      tw_text_idx = (tw_row + 1) * tw->cols + fe->line_num_w;
    }
  }
}

bool FRED_render_text(TermWin* tw, Cursor* cursor, short line_num_w)
{
  bool failed = 0;
  fprintf(stdout, "\x1b[H");
  fwrite(tw->text, sizeof(*tw->text), tw->size, stdout);
  fprintf(stdout, "\033[%zu;%zuH", cursor->win_row + 1, line_num_w + cursor->win_col + 1);
  fflush(stdout);
  GOTO_END(failed);
end:
  return failed;
}


bool FRED_make_piece(FredEditor* fe, char key)
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
  FRED_win_resize(&tw);

  while (running) {
    FRED_get_text_to_render(fe, &tw, insert);
    FRED_render_text(&tw, &fe->cursor, fe->line_num_w);

    ssize_t b_read = read(STDIN_FILENO, &key, 10);
    if (b_read == -1) {
      if (errno == EINTR){
        FRED_win_resize(&tw);
        continue;
      }
      ERROR("couldn't read from stdin");
    }

    if (insert) {
      if (key == ESC_CH) insert = false;

      if (key == '\n' || (key >= ' ' && key <= '~')){
        FRED_make_piece(fe, key);
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
        // case 'w': { FRED_save_file(fe, file_path); break;}
        case 'q': { running = false; break;}
        // case 'i': { insert = true; break;}

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

// FIXME: SIGWINCH doesn't handle resizing on zooming in/out when
// the terminal is not full screen?

// FIXME: when resizing the window i need to reposition the win_curs 

// FIXME: fred sometimes crashes with empty file even though that should already be handled
// TODO: fred_get_text_from_piece_table() is doing unrelated things either
// call it get_all_text or move something out
// TODO: add testing
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
