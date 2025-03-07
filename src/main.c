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
#include "utils.h"


termios term_orig = {0};
struct sigaction act = {0};
struct sigaction old = {0};



// NOTE: This is for debugging, ignore it
void printing(void)
{
  tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
  // sigaction(SIGWINCH, &old, NULL);
  fprintf(stdout, "\033[2J\033[H");
}




bool FRED_open_file(FileBuf* file_buf, const char* file_path)
{
  bool failed = 0;
  bool file_loaded = 0;

  struct stat sb;
  if (stat(file_path, &sb) == -1) {
    if (errno == ENOENT) ERROR("no such file.");
    else ERROR("failed to retrieve any info about file '%s'. %s.", file_path, strerror(errno));
  }

  if ((sb.st_mode & S_IFMT) != S_IFREG) {
    if ((sb.st_mode & S_IFMT) == S_IFDIR) {
      ERROR("path '%s' is a directory. Please provide a path to a file.", file_path);
    } 
    ERROR("file '%s' is not a regular file.", file_path);
  }

  int fd = open(file_path, O_RDONLY);
  if (fd == -1) ERROR("failed to open file '%s'. %s.", file_path, strerror(errno));

  file_buf->size = sb.st_size;
  file_buf->text = malloc(sizeof(*file_buf->text) * file_buf->size);
  if (file_buf->text == NULL) ERROR("not enough memory for file file-buffer.");
  file_loaded = 1;

  ssize_t bytes_read = read(fd, file_buf->text, file_buf->size);
  if (bytes_read == -1) ERROR("failed to read file '%s'. %s", file_path, strerror(errno));

  while ((size_t) bytes_read < file_buf->size) {
    ssize_t new_bytes_read = read(fd, file_buf->text + bytes_read, file_buf->size - bytes_read);
    if (new_bytes_read == -1) {
      ERROR("failed to read file '%s'. %s.", file_path, strerror(errno));
    }
    bytes_read += new_bytes_read;
  }

  if (close(fd) == -1) {
    ERROR("failed to close file '%s'. %s.", file_path, strerror(errno));
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
    ERROR("failed to open '%s' while trying save it. %s.", file_path, strerror(errno));
  }

  if (file_size > 0){
    for (size_t j = 0; j < fe->piece_table.len; j++){
      Piece* piece = fe->piece_table.items + j;
      char* buf = !piece->which_buf ? fe->file_buf.text : fe->add_buf.items;
      fwrite(buf + piece->offset, sizeof(*buf), piece->len, f);
    }
  } else {
    int fd = fileno(f);
    if (-1 == ftruncate(fd, 0)) {
      ERROR("failed to truncate file while trying to save emptied out file. %s.", strerror(errno));
    }
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
    ERROR("Fred editor can only be run in a terminal.");
  }
  
  if (tcgetattr(STDIN_FILENO, &term_orig) == -1){
    ERROR("failed to retrieve terminal options. %s.", strerror(errno));
  }

  termios term_raw = term_orig;
  term_raw.c_iflag &= ~(BRKINT | ISTRIP | INPCK |  IXON); // ICRNL |
  term_raw.c_lflag &= ~(ICANON | IEXTEN | ECHO);
  term_raw.c_cflag &= ~(CSIZE | PARENB);
  term_raw.c_cflag |= CS8;
  term_raw.c_cc[VTIME] = 5;

  if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_raw)){
    ERROR("failed to set terminal options. %s.", strerror(errno));
  }

  act.sa_handler = handle_win_resize_sig;
  if (-1 == sigaction(SIGWINCH, &act, &old)){
    ERROR("failed to set up the editor to detect window changes. %s.", strerror(errno));
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
  fe->last_edit = (LastEdit){0};

  GOTO_END(failed);
end:
  if (failed){
    if (file_loaded) free(fe->file_buf.text);

    if (term_and_sig_set) {
      tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
      sigaction(SIGWINCH, &old, NULL);
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
    ERROR("failed to retrieve terminal size. %s.", strerror(errno));
  }
  
  tw->size = w.ws_row * w.ws_col;
  tw->rows = w.ws_row;
  tw->cols = w.ws_col;
  void* temp = realloc(tw->text, tw->size);
  if (temp == NULL) ERROR("not enough memory to get and display text.");
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

  size_t last_endline_idx = 0; // 'idx' in the text constructed from the piece-table
  size_t tot_text_len = 0;
  size_t tw_text_row_width = tw->cols - tw->line_num_w;

  size_t tot_rows = 0;
  size_t tw_lines_rows_idx = 0;
  int tw_lines_rows[tw->rows - 1];
  memset(&tw_lines_rows, -1, tw->rows - 1);

  // TODO: This thing should happen after move_cursor, as the rows-scrolling
  // should be done when editing as well.
  for (size_t piece_idx = 0; piece_idx < fe->piece_table.len; piece_idx++){
    Piece* piece = fe->piece_table.items + piece_idx;
    char* buf = !piece->which_buf ? fe->file_buf.text : fe->add_buf.items;

    for (size_t i = 0; i < piece->len; i++){
      size_t buf_idx = piece->offset + i;
      if (buf[buf_idx] != '\n' && 
         (piece_idx < fe->piece_table.len - 1 || i < piece->len - 1)) continue;

      size_t endline_idx = tot_text_len + i + 1;
      size_t line_len = endline_idx - last_endline_idx - (buf[buf_idx] == '\n'? 1 : 0);
      last_endline_idx = endline_idx;

      if (tot_lines >= tw->lines_to_scroll && tot_rows < tw->rows - 1){
        size_t line_wrapped_rows = (line_len + tw_text_row_width - line_len % tw_text_row_width) / tw_text_row_width;
        tot_rows += line_wrapped_rows;
        tw_lines_rows[tw_lines_rows_idx++] = line_wrapped_rows;
      }

      if (tot_lines == fe->cursor.row) curs_line_len = line_len;
      else if (tot_lines == fe->cursor.row - 1) curs_up_line_len = line_len;
      else if (tot_lines == fe->cursor.row + 1) curs_down_line_len = line_len;

      tot_lines++;
    }
    tot_text_len += piece->len;
  }

  // NOTE: cursor.win_row/win_col is repositioned in fred_grab_text_from_piece_table()
  // because it's easier doing it there when the window gets resized/zoomed 
  
  // TODO: by saving last_edit_pos, i can save the lines informations and not have
  // to get them everytime if a new edit has been done.
  
  switch (key){
    case 'j': {
      if (fe->cursor.row + 1 >= tot_lines) return; 

      fe->cursor.row++;
      if (fe->cursor.col > curs_down_line_len){
        fe->cursor.col = curs_down_line_len;
      }

      size_t curs_line_rows = (curs_line_len + tw_text_row_width - curs_line_len % tw_text_row_width) / tw_text_row_width;
      if (fe->cursor.win_row + curs_line_rows < tw->rows / 2 + 5) return;

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
      return;
    }

    case 'k': {
      if ((int)fe->cursor.row - 1 < 0) return;

      size_t curs_up_line_rows = (curs_up_line_len + tw_text_row_width - curs_up_line_len % tw_text_row_width) / tw_text_row_width; 
      if (fe->cursor.col > curs_up_line_len){
        fe->cursor.col = curs_up_line_len;
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
      if (fe->cursor.col + 1 > curs_line_len) return;
      fe->cursor.col++;
      return;
    }
  }
}


void FRED_get_text_to_render(FredEditor* fe, TermWin* tw, bool insert)
{
  memset(tw->text, SPACE_CH, tw->size);

  size_t last_row_idx = (tw->rows - 1) * tw->cols + 1;
  char* mode = insert ? "-- INSERT --" : "-- NORMAL --";
  memcpy(tw->text + last_row_idx, mode, strlen(mode));
  TW_WRITE_NUM_AT(tw, last_row_idx + tw->cols - 1, "%-ld:%-ld", fe->cursor.row + 1,fe->cursor.col + 1); 
  TW_WRITE_NUM_AT(tw, tw->line_num_w - tw->line_num_w / 3, "%ld", tw->lines_to_scroll + 1); // first line-num

  size_t tw_text_idx = tw->line_num_w;
  size_t line = 0;
  bool win_curs_set = false;
  #define IS_NOT_EOF_NEWLINE (!(!piece->which_buf && piece_idx == fe->piece_table.len - 1 && i == piece->len - 1))

  for (size_t piece_idx = 0; piece_idx < fe->piece_table.len; piece_idx++){
    Piece* piece = &fe->piece_table.items[piece_idx];
    char* buf = !piece->which_buf ? fe->file_buf.text : fe->add_buf.items;

    for (size_t i = 0; i < piece->len; i++){
      char c = buf[piece->offset + i];
      size_t tw_row = tw_text_idx / tw->cols;
      size_t tw_col = tw_text_idx % tw->cols;

      if (tw_row >= tw->rows - 1) return;
      if (line < tw->lines_to_scroll){
        if (c == '\n') line++;
        continue;
      }

      if (c != '\n'){
        if (tw_col == 0) tw_text_idx += tw->line_num_w;
        tw->text[tw_text_idx++] = c;
      } else {
        line++;
        tw_text_idx = (tw_row + 1) * tw->cols + tw->line_num_w;
        if (tw_row + 1 < tw->rows - 1 && IS_NOT_EOF_NEWLINE) {
          TW_WRITE_NUM_AT(tw, tw_text_idx - tw->line_num_w / 3, "%ld", line + 1);
        }
      }

      if (!win_curs_set && line == fe->cursor.row){
        win_curs_set = true;
        size_t tw_text_row_w = tw->cols - tw->line_num_w;
        fe->cursor.win_col = fe->cursor.col % tw_text_row_w;
        fe->cursor.win_row = tw_row + (c == '\n') + 
          (fe->cursor.col - fe->cursor.col % tw_text_row_w) / tw_text_row_w;
      }
    }
  }
  #undef IS_NOT_EOF_NEWLINE
}

bool FRED_render_text(TermWin* tw, Cursor* cursor)
{
  bool failed = 0;
  fprintf(stdout, "\x1b[H");
  fwrite(tw->text, sizeof(*tw->text), tw->size, stdout);
  fprintf(stdout, "\033[%zu;%zuH", cursor->win_row + 1, tw->line_num_w + cursor->win_col + 1);
  fflush(stdout);
  GOTO_END(failed);
end:
  return failed;
}



bool FRED_insert_text(FredEditor* fe, char text_char)
{
#define CURSOR_AT_LAST_EDIT_POS (fe->cursor.row == fe->last_edit.cursor.row && fe->cursor.col == fe->last_edit.cursor.col)

  bool failed = 0;

  ADD_BUF_PUSH(&fe->add_buf, text_char);
  int col = fe->cursor.col;
  size_t add_buf_len = fe->add_buf.len;
  size_t line = 0;
  PieceTable* pt = &fe->piece_table;

  if (pt->len == 0){
    PIECE_TABLE_MAKE_ROOM(pt, pt->items, 0, 0, 0);
    PIECE_TABLE_INSERT(pt, pt->items[pt->len], 1, fe->add_buf.len - 1, 1);
    goto end_loop;
  }

  for (size_t pi = 0; pi < pt->len; pi++){
    Piece* piece = &pt->items[pi];
    char* buf = !piece->which_buf? fe->file_buf.text : fe->add_buf.items;

    for (size_t j = 0; j < piece->len; j++){
      char c = buf[piece->offset + j];
      if (line < fe->cursor.row){
        if (c == '\n') line++;
      } else if (col > 0){
        col--;
      } 
      if (line < fe->cursor.row || col > 0) continue;

      if (fe->cursor.row == 0 && fe->cursor.col == 0){ // add at table-start
        PIECE_TABLE_MAKE_ROOM(pt, piece, 1, 0, pt->len - pi + 1);
        PIECE_TABLE_INSERT(pt, piece[0], 1, add_buf_len - 1, 1);
        goto end_loop;
      }

      if (j + 1 >= piece->len){ // insertion at end of piece
        if (CURSOR_AT_LAST_EDIT_POS && piece->which_buf && fe->last_edit.action == ACT_INSERT){
          piece->len++; // TODO: just cache this piece
          goto end_loop;
        }
        if (pi + 1 >= pt->len){
          PIECE_TABLE_INSERT(pt, pt->items[pt->len], 1, add_buf_len - 1, 1);
          goto end_loop;
        }
        PIECE_TABLE_MAKE_ROOM(pt, piece, 2, 1, pt->len - pi + 1);
        PIECE_TABLE_INSERT(pt, piece[1], 1, add_buf_len - 1, 1);
        goto end_loop;
      }
      size_t p1_len = j + 1;
      size_t p3_offset = piece->offset + p1_len;
      size_t p3_len = piece->len - p1_len;

      // TODO: it's not very clear what's going on if i pass piece-fields like this 
      piece->len = p1_len;
      PIECE_TABLE_MAKE_ROOM(pt, piece, 3, 1, pt->len - pi + 1);
      PIECE_TABLE_INSERT(pt, piece[1], 1, add_buf_len - 1, 1);
      PIECE_TABLE_INSERT(pt, piece[2], piece->which_buf, p3_offset, p3_len);
      goto end_loop;
    }
  }
end_loop:
  if (text_char == '\n'){
    fe->cursor.row++;
    fe->cursor.col = 0; 
  } else {
    fe->cursor.col++; 
  }
  fe->last_edit.cursor = fe->cursor;
  fe->last_edit.action = ACT_INSERT;

  GOTO_END(failed);
end:
  return failed;
  #undef CURSOR_AT_LAST_EDIT_POS
}


bool FRED_delete_text(FredEditor* fe)
{
  bool failed = 0;

  if (fe->cursor.row == 0 && fe->cursor.col == 0) return failed;

  PieceTable* pt = &fe->piece_table;
  int col = fe->cursor.col;
  size_t line = 0;
  size_t curs_up_line_len = 0;
  char deleted_char = 0;

  for (size_t pi = 0; pi < pt->len; pi++){
    Piece* piece = &pt->items[pi];
    char* buf = !piece->which_buf? fe->file_buf.text : fe->add_buf.items;

    for (size_t j = 0; j < piece->len; j++){
      char c = buf[piece->offset + j];
      if (line < fe->cursor.row){
        if (c == '\n') line++;
        else if (line == fe->cursor.row - 1) curs_up_line_len++;
      } else if (col > 0){
        col--;
      }
      if (line < fe->cursor.row || col > 0) continue;

      deleted_char = c;

      if (j + 1 >= piece->len){
        if (piece->len - 1 > 0){
          piece->len--;
        } else {
          memmove(piece, piece + 1, (pt->len - pi + 1) * sizeof(*piece));
          fe->piece_table.len--;
        }
        goto end_loop;
      }
      // deletion inside a piece
      PIECE_TABLE_MAKE_ROOM(pt, piece, 1, 0, pt->len - pi + 1);
      PIECE_TABLE_INSERT(pt, piece[1], piece->which_buf, piece->offset + j + 1, piece->len - j - 1);
      piece->len = j;
      goto end_loop;
    }
  }

end_loop:
  if (deleted_char == '\n'){
    fe->cursor.row--;
    fe->cursor.col = curs_up_line_len; 
  } else {
    fe->cursor.col--; 
  }
  fe->last_edit.cursor = fe->cursor;
  fe->last_edit.action = ACT_DELETE;
end:
  return failed;
  #undef CURSOR_AT_LAST_EDIT_POS
}



void dump_piece_table(FredEditor* fe)
{
  for (size_t i = 0; i < fe->piece_table.len; i++){
    Piece piece = fe->piece_table.items[i];
    char* buf = !piece.which_buf ? 
                      fe->file_buf.text + piece.offset: 
                      fe->add_buf.items + piece.offset;
    fprintf(stdout, "piece at [%ld] = {buf = %d, offset = %lu, len = %lu}\n", 
            i, piece.which_buf, piece.offset, piece.len);

    for (size_t j = 0; j < piece.len; j++){
      fprintf(stdout, "%c %d\n", buf[j], (int)buf[j]);
    }
    fprintf(stdout, "----------------------------------------------------------------------\n");
  }
}


bool FRED_start_editor(FredEditor* fe, const char* file_path)
{
  bool failed = 0;
  bool running = true;
  bool insert = false;

  TermWin tw = {0};
  tw.line_num_w = 8;
  failed = FRED_win_resize(&tw);
  if (failed) GOTO_END(1);

  while (running) {
    FRED_get_text_to_render(fe, &tw, insert);
    FRED_render_text(&tw, &fe->cursor);

    char key[MAX_KEY_LEN] = {0};
    ssize_t b_read = read(STDIN_FILENO, key, MAX_KEY_LEN);
    if (b_read == -1) {
      if (errno == EINTR){
        failed = FRED_win_resize(&tw);
        if (failed) GOTO_END(1);
        continue;
      }
      ERROR("failed to read from stdin");
    }


    if (insert){
      if (KEY_IS(key, "\x1b") || KEY_IS(key, "\x1b ")){
        fe->last_edit.cursor = fe->cursor;
        insert = false;
      } else if (KEY_IS(key, "\x7f")){
        failed = FRED_delete_text(fe);
        if (failed) GOTO_END(1);
        
      } else{
        ASSERT_MSG(b_read == 1, "UTF-8 not supported yet. Typed: '%s'", key);
        failed = FRED_insert_text(fe, key[0]);
        if (failed) GOTO_END(1);
      }
      
    } else {
      if (KEY_IS(key, "h") || 
          KEY_IS(key, "j") || 
          KEY_IS(key, "k") || 
          KEY_IS(key, "l")) FRED_move_cursor(fe, &tw, key[0]);

      else if (KEY_IS(key, "q")) running = false;
      else if (KEY_IS(key, "i")) insert = true;
      else if (KEY_IS(key, "\x1b") || KEY_IS(key, "\x1b ")) insert = false;
    }
  }
  GOTO_END(failed);

end:
  if (!failed) { // else the ERROR() macro has lready cleared the screen
    fprintf(stdout, "\033[2J\033[H");
  }

  tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
  sigaction(SIGWINCH, &old, NULL);
  // dump_piece_table(fe);

  fred_editor_free(fe);
  free(tw.text);
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


// TODO: write test. IN neovim record all the keypresses while editing a 
// file with only Fred's controls and write them onto a file. 
// Feed these data to Fred's function and test if the edited file
// and Fred's output are the same. 
// While recording I could even random randomize the keypresses
// instead of writing it myself: just have an array of fred's controls,
// get random control and random repetitions amount.


// TODO: what if i just make the test.c a module that i will call 
// directly from main.c on when testing? I wouldn't need to separate
// everything into modules then. Maybe in the main I can have a flag or something 
// that i can pass to the console?
// FIXME: it happened only once, but after pressing 'k' in 
// normal mode some of the deleted pieces got rendered again.
// FIXME: win_row cannot scroll back to line-1 if (file->lines < tw->rows - 1), but you're technically there 
// and if you type some text wou will get repositioned at the correct spot.
// TODO+FIXME!!!!: the lines-scrolling code should be independant
// from the code that handles cursor_move up/down.
// lines should be scrolled also when editing text past the screen
// threeshold.
// FIXME: SIGWINCH doesn't handle resizing on zooming in/out when
// the terminal is not full screen?
// FIXME: when zooming in too much, if the cursor win_row is greater than
// the tw->rows the the lines should get scrolled.
// FIXME: fred sometimes crashes with empty file even though that should already be handled
//
// TODO: can i like have a bit-flag  that says if I'm at the end or start of table/line or sum? 
// TODO: when scrolling down, i could save a pointer to the 
// piece that contatins the first line to render in TermWin, so I won't have
// to parse the entire table when rendering.
// TODO: utf-8 support.
// TODO: if any of the realloc's (win_resize, DA_macros) were to fail,
// since the contents should not be touched, I think the user 
// should be prompted whether or not to write the all the edits 
// to the file. But can you actually do that? 
// TODO: handle ftruncate error
// TODO: have a separate buffer to report error messages in the 
// bottom line of the screen
// TODO: handle eintr on close() in open_file()
// TODO: handle failures in render_text()
// TODO: in the future if i decide to add custom key-maps, I could 
// use an array with designated initializers to hold mappings
// mappings = { [ESC_KEY] = void (*some_job)(void*)...,
             // [SPACE_KEY] = void (*some_job)(void*)...,}
// where ESC_KEY is a preproc constant


// For the general structure of the project: 
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
