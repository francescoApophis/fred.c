#include "fred.h"


bool FRED_open_file(FileBuf* file_buf, const char* file_path)
{

  bool failed = 0;
  bool file_loaded = 0;

  struct stat sb;
  if (stat(file_path, &sb) == -1) {
    if (errno == ENOENT) ERROR("no file '%s' found.", file_path);
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






bool fred_editor_init(FredEditor* fe, const char* file_path)
{
  bool failed = 0;
  bool file_loaded = 0;

  DA_INIT(&fe->piece_table);
  DA_INIT(&fe->add_buf);
  DA_INIT(&fe->lines_len);

  failed = FRED_open_file(&fe->file_buf, file_path);
  if (failed) GOTO_END(1);
  file_loaded = 1;

  if (fe->file_buf.size > 0){
    PIECE_TABLE_PUSH(&fe->piece_table, ((Piece){
      .which_buf = 0,
      .offset = 0,
      .len = fe->file_buf.size,
    }));
  }

  fe->cursor = (Cursor){0};
  fe->last_edit = (LastEdit){0};

  GOTO_END(failed);
end:
  if (failed){
    if (file_loaded) free(fe->file_buf.text);
  }
  return failed;
}

void fred_editor_free(FredEditor* fe)
{
  DA_FREE(&fe->piece_table, true);
  DA_FREE(&fe->add_buf, true);
  DA_FREE(&fe->lines_len, true);
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
    Piece new_piece = {.which_buf = 1, .offset = add_buf_len - 1, .len = 1};
    PIECE_TABLE_PUSH(pt, new_piece);
    goto end_loop;
  }

  if (fe->cursor.row == 0 && fe->cursor.col == 0){ // add at table-start
    Piece new_piece = {.which_buf = 1, .offset = add_buf_len - 1, .len = 1};
    PIECE_TABLE_MAKE_ROOM(pt, 0, 1, pt->len);
    PIECE_TABLE_INSERT(pt, 0, new_piece);
    goto end_loop;
  }

  for (size_t piece_idx = 0; piece_idx < pt->len; piece_idx++){
    Piece* piece = &pt->items[piece_idx];
    char* buf = !piece->which_buf? fe->file_buf.text : fe->add_buf.items;

    for (size_t j = 0; j < piece->len; j++){
      char c = buf[piece->offset + j];
      if (line < fe->cursor.row){
        if (c == '\n') line++;
      } else if (col > 0){
        col--;
      } 
      if (line < fe->cursor.row || col > 0) continue;

      if (j + 1 >= piece->len){ // insertion at end of piece
        if (CURSOR_AT_LAST_EDIT_POS && piece->which_buf && fe->last_edit.action == ACT_INSERT){
          piece->len++;
          goto end_loop;
        }
        if (piece_idx + 1 >= pt->len){
          Piece new_piece = {.which_buf = 1, .offset = add_buf_len - 1, .len = 1};
          DA_MAYBE_GROW(pt, 1, PIECE_TABLE_INIT_CAP, PieceTable);
          PIECE_TABLE_INSERT(pt, pt->len, new_piece);
          goto end_loop;
        }
        Piece new_piece = {.which_buf = 1, .offset = add_buf_len - 1, .len = 1};
        PIECE_TABLE_MAKE_ROOM(pt, piece_idx + 1, piece_idx + 2, pt->len - (piece_idx + 1));
        PIECE_TABLE_INSERT(pt, piece_idx + 1, new_piece);
        goto end_loop;
      }

      // insertion in the middle of a piece
      size_t p1_len = j + 1; // because 'j' is 0-indexed
      size_t p3_offset = piece->offset + p1_len;
      size_t p3_len = piece->len - p1_len;
      
      PIECE_TABLE_MAKE_ROOM(pt, piece_idx + 1, piece_idx + 3, pt->len - (piece_idx + 1));

      PIECE_TABLE_INSERT(pt, piece_idx + 1, ((Piece){ 
        .which_buf = 1, 
        .offset = add_buf_len - 1,
        .len = 1 
      }));

      PIECE_TABLE_INSERT(pt, piece_idx + 2, ((Piece){ 
        .which_buf = piece->which_buf, 
        .offset = p3_offset, 
        .len = p3_len,
      }));
      piece->len = p1_len;
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
        if (piece->len - 1 > 0){ // FIXME: just check if (piece->len)
          piece->len--;
        } else {
          memmove(piece, piece + 1, (pt->len - (pi + 1)) * sizeof(*piece));
          fe->piece_table.len--;
        }
        goto end_loop;
      }
      // deletion inside a piece
      PIECE_TABLE_MAKE_ROOM(pt, pi + 1, pi + 2, pt->len - (pi + 1));

      Piece new_piece = {
        .which_buf = piece->which_buf, 
        .offset = piece->offset + j + 1, 
        .len = piece->len - j - 1,
      };
      PIECE_TABLE_INSERT(pt, pi + 1, new_piece);

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



void dump_piece_table(FredEditor* fe, FILE* stream)
{

  fprintf(stream, "LINES-LENGHTS:\n");
  for (size_t i = 0; i < fe->lines_len.len; i++){
    fprintf(stream, "[%ld] = %d:\n", i + 1, fe->lines_len.items[i]);
  }

  fprintf(stream, "TABLE:\n");
  for (size_t i = 0; i < fe->piece_table.len; i++){
    Piece piece = fe->piece_table.items[i];
    char* buf = (!piece.which_buf ? fe->file_buf.text : fe->add_buf.items) + piece.offset;
    fprintf(stream, "[%ld] => [buf = %d, offset = %ld, len = %ld]:\n", 
            i, piece.which_buf, piece.offset, piece.len);

    fprintf(stream, "%.*s\n", (int)piece.len, buf);

    fprintf(stream, "----------------------------------------------------------------------\n");
  }
}


// NOTE: this computes all lines's length
// as well as lines-count (LinesLen->len) for 
// navigation in normal mode
bool FRED_get_lines_len(FredEditor* fe)
{
#define is_last_char(t, p, i, j) ((i) == (t)->len - 1 && (j) == (p)->len - 1)

  bool failed = 0;
  PieceTable* t = &fe->piece_table;
  LinesLen* ll = &fe->lines_len;
  
  ll->len = 0;
  size_t line_start = 0;
  size_t tot_text_len = 0;

  for (size_t i = 0; i < t->len; i++) {
    Piece* p = &t->items[i];
    char* buf = p->offset + (!p->which_buf ? fe->file_buf.text : fe->add_buf.items);

    for (size_t j = 0; j < p->len; j++) {
      char c = buf[j];
      if (c == '\n' || is_last_char(t, p, i, j)) {
        // NOTE: we count the length from +1 past the end of the string
        size_t line_end = tot_text_len + j + (is_last_char(t, p, i, j) && c != '\n');
        ASSERT(line_end - line_start <= UINT16_MAX, 
               "line-length overflow (max is UINT16_MAX, 65535), length cannot be stored for later usage");
        DA_PUSH(ll, (uint16_t)(line_end - line_start), 8, LinesLen);
        line_start = line_end + 1;
      }
    }
    tot_text_len += p->len;
  }
end:
  return failed;
#undef is_last_char
}


void FRED_move_cursor(FredEditor* fe, char key) 
{
  Cursor* cr = &fe->cursor;
  cr->prev_row = cr->row;
  cr->prev_col = cr->col;
  size_t tot_lines = fe->lines_len.len;

  if (!tot_lines) return;

  switch (key){
    case 'h': {
      if ((int)cr->col - 1 < 0) return;
      cr->col--;
      return;
    } 
    case 'l': {
      size_t curr_line_len = tot_lines == 0 ? 0 : fe->lines_len.items[cr->row];
      if (cr->col + 1 > curr_line_len) return;
      cr->col++;
      return;
    }
    case 'j': {
      if (cr->row >= tot_lines - 1) return;
      cr->row++;
      size_t line_len = fe->lines_len.items[cr->row];
      if (cr->col > line_len) {
        cr->col = line_len;
      }
      return;
    }
    case 'k': {
      if ((int64_t)cr->row - 1 < 0) return;
      cr->row--;
      size_t line_len = fe->lines_len.items[cr->row];
      if (cr->col > line_len) {
        cr->col = line_len;
      }
      return;
    }
  }
}

void update_win_cursor(FredEditor* fe, TermWin* tw)
{
  LinesLen* ll = &fe->lines_len;
  if (ll->items == NULL || !ll->len) return;

  Cursor* cr = &fe->cursor;
  size_t tw_row_w = tw->cols - tw->line_num_w;

  size_t win_col = cr->col % tw_row_w;
  cr->win_col = win_col;

  size_t mid = tw->rows * 0.5;

  if (cr->win_row > mid + 5) {
    size_t curr_line_rows = ll->items[cr->row] / tw_row_w + 1;
    size_t rows = ll->items[tw->lines_to_scroll++] / tw_row_w + 1;
    // NOTE: the 2nd check will render the current line closer the center if it's wrapped
    while (rows < curr_line_rows || (curr_line_rows > 1 && rows <= curr_line_rows)) {
      rows += ll->items[++tw->lines_to_scroll] / tw_row_w + 1;
    }
  } else if (tw->lines_to_scroll && cr->win_row < mid - 5) {
    size_t prev_line_rows = ll->items[cr->prev_row] / tw_row_w + 1;
    size_t rows = ll->items[--tw->lines_to_scroll] / tw_row_w + 1;
    while (tw->lines_to_scroll && rows < prev_line_rows) {
      rows += ll->items[--tw->lines_to_scroll] / tw_row_w + 1;
    }
  }

  // NOTE: loops from the 1st line on the screen
  size_t win_row = 0;
  for (size_t i = tw->lines_to_scroll; i < cr->row; i++) {
    size_t line_len = ll->items[i];
    if (line_len < tw_row_w) win_row++;
    else win_row += line_len / tw_row_w + 1;
  }
  cr->win_row = win_row + cr->col / tw_row_w;
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

  FRED_get_lines_len(fe);

  while (running) {
    FRED_get_text_to_render(fe, &tw, insert);
    FRED_render_text(&tw, &fe->cursor);

    char key[MAX_KEY_LEN] = {0};
    ssize_t b_read = read(STDIN_FILENO, key, MAX_KEY_LEN);
    if (b_read == -1) {
      if (errno == EINTR){
        failed = FRED_win_resize(&tw);
        if (failed) GOTO_END(1);
        update_win_cursor(fe, &tw);
        continue;
      }
      ERROR("failed to read from stdin");
    }

    if (insert){
      fe->cursor.prev_row = fe->cursor.row;
      fe->cursor.prev_col = fe->cursor.col;

      if (KEY_IS(key, "\x1b") || KEY_IS(key, "\x1b ")){ // escape
        fe->last_edit.cursor = fe->cursor;
        insert = false;
      } else if (KEY_IS(key, "\x7f")){
        failed = FRED_delete_text(fe);
        if (failed) GOTO_END(1);
        
      } else{
        // ASSERT_MSG(b_read == 1, "UTF-8 not supported yet. Typed: '%s'", key);
        if (b_read == 1) {
          failed = FRED_insert_text(fe, key[0]);
          if (failed) GOTO_END(1);
        }
      }

      FRED_get_lines_len(fe);
      update_win_cursor(fe, &tw);

    } else {
      if (KEY_IS(key, "h") || KEY_IS(key, "j") || 
          KEY_IS(key, "k") || KEY_IS(key, "l"))  {
        FRED_move_cursor(fe, key[0]);
        update_win_cursor(fe, &tw);

      } else if (KEY_IS(key, "q")) running = false;
        else if (KEY_IS(key, "i")) insert = true;
        else if (KEY_IS(key, "\x1b") || KEY_IS(key, "\x1b ")) insert = false;
    }
  }
  GOTO_END(failed);

end:
  if (!failed) { // else the ERROR() macro has already cleared the screen
    fprintf(stdout, "\033[2J\033[H");
  }

  dump_piece_table(fe, stdout);
  fred_editor_free(fe);
  free(tw.text);
  return failed;
}
