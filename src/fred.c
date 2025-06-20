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

  // TODO: just use fread?

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
#define buf(p, offset)((!(p).which_buf ? fe->file_buf.text: fe->add_buf.items)[(offset)])

  memset(tw->text, SPACE_CH, tw->size);

  size_t last_row_idx = (tw->rows - 1) * tw->cols + 1;
  char* mode = insert ? "-- INSERT --" : "-- NORMAL --";
  memcpy(tw->text + last_row_idx, mode, strlen(mode));
  TW_WRITE_NUM_AT(tw, last_row_idx + tw->cols - 1, "%-d:%-d", (int)fe->cursor.row + 1, (int)fe->cursor.col + 1); 
  TW_WRITE_NUM_AT(tw, tw->line_num_w - tw->line_num_w / 3, "%ld", tw->lines_to_scroll + 1); // first line-num

  PieceTable* table = &fe->piece_table;
  LinesLen* ll = &fe->lines_len;

  if (table->len == 0) return;

  // TODO: could i cache it?  
  size_t fl_offset = 0; // [f]irst [l]ine to render; offset in the fully built text
  for (size_t i = 0; i < tw->lines_to_scroll; i++) {
    fl_offset += ll->items[i] + 1; // NOTE: '+1' is for '\n' 
  }

  size_t fl_piece_idx = 0, fl_piece_offset = 0;
  if (fl_offset > 0) {
    for (size_t i = 0, pieces_len = 0; i < table->len; i++) {
      Piece p = table->items[i];
      pieces_len += p.len;

      if (!(pieces_len < fl_offset)) {
        fl_piece_idx = i;
        fl_piece_offset = p.len - (pieces_len - fl_offset);
        break;
      }
    }
  }


  size_t tw_text_idx = 0, line = tw->lines_to_scroll;
  size_t limit = tw->size - tw->cols * 2;
  size_t off = tw->line_num_w / 3;
  for (size_t i = fl_piece_idx; i < table->len; i++) {
    Piece p = table->items[i];

    size_t j = i == fl_piece_idx ? fl_piece_offset : 0; 
    for (; j < p.len; j++) {
      if (tw_text_idx >= limit) return;

      char c = buf(p, p.offset + j);
      size_t tw_col = tw_text_idx % tw->cols;

      if (c != '\n'){
        if (tw_col == 0) tw_text_idx += tw->line_num_w;
        tw->text[tw_text_idx++] = c;
      } else {
        line++;
        tw_text_idx += (tw->cols - tw_col) + tw->line_num_w;
        TW_WRITE_NUM_AT(tw, tw_text_idx - off, "%ld", line + 1);
      }
    }
  }

#undef buf
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

bool insert_at_table_start(FredEditor* fe)
{
  bool failed = 0;
  PieceTable* table = &fe->piece_table;
  DA_MAYBE_GROW(table, 1, PIECE_TABLE_INIT_CAP, PieceTable);
  memmove(table->items + 1, table->items, table->len * sizeof(*table->items));
  table->items[0] = (Piece) {
    .which_buf = 1,
    .offset = fe->add_buf.len - 1,
    .len = 1,
  };
  table->len++;
end:
  return failed;
}


bool insert_after_piece(FredEditor* fe, size_t piece_idx)
{
#define AT_LAST_EDIT_POS (fe->cursor.row == fe->last_edit.cursor.row && fe->cursor.col == fe->last_edit.cursor.col)
#define LAST_ACT_WAS_INSERT (fe->last_edit.action == ACT_INSERT)
#define IS_ADD_BUF_PIECE (table->items[piece_idx].which_buf)

  bool failed = 0;
  PieceTable* table = &fe->piece_table;

  if (AT_LAST_EDIT_POS && LAST_ACT_WAS_INSERT && IS_ADD_BUF_PIECE) {
    table->items[piece_idx].len++;
  } else {
    DA_MAYBE_GROW(table, 1, PIECE_TABLE_INIT_CAP, PieceTable);
    size_t n = (table->len - (piece_idx + 1)) * sizeof(*table->items);
    memmove(&table->items[piece_idx+2], &table->items[piece_idx+1], n);
    table->items[piece_idx + 1] = (Piece) {
      .which_buf = 1,
      .offset = fe->add_buf.len - 1,
      .len = 1,
    };
    table->len++;
  }
end:
  return failed;

#undef AT_LAST_EDIT_POS
#undef LAST_ACT_WAS_INSERT
#undef IS_ADD_BUF_PIECE
}

bool insert_inside_piece(FredEditor* fe, size_t piece_idx, size_t edit_offset) 
{
  bool failed = 0;

  PieceTable* table = &fe->piece_table;
  Piece curr_piece = table->items[piece_idx];
  size_t piece_1_len = edit_offset; 
  size_t piece_3_offset = curr_piece.offset + piece_1_len;
  size_t piece_3_len = curr_piece.len - piece_1_len;
 
  DA_MAYBE_GROW(table, 2, PIECE_TABLE_INIT_CAP, PieceTable);
  size_t n = (table->len - (piece_idx + 1)) * sizeof(*table->items);
  memmove(&table->items[piece_idx + 3], &table->items[piece_idx + 1], n);

  table->items[piece_idx + 1] = (Piece){ 1, fe->add_buf.len - 1, 1};
  table->items[piece_idx + 2] = (Piece){ curr_piece.which_buf, piece_3_offset, piece_3_len};
  table->items[piece_idx].len = piece_1_len;
  table->len += 2;
end:
  return failed;
}


bool FRED_insert_text(FredEditor* fe, char text_char)
{
#define LAST_ACT_WAS_INSERT (fe->last_edit.action == ACT_INSERT)
#define AT_LAST_LINE_END (cr->row == ll->len - 1 && cr->col == (size_t)ll->items[cr->row])
  bool failed = 0;

  PieceTable* table = &fe->piece_table;
  LinesLen* ll = &fe->lines_len;
  Cursor* cr = &fe->cursor;

  ADD_BUF_PUSH(&fe->add_buf, text_char);

  if (table->len == 0 || (!LAST_ACT_WAS_INSERT && AT_LAST_LINE_END)) {
    PIECE_TABLE_PUSH(table, ((Piece){1, fe->add_buf.len - 1, 1}));
    GOTO_END(failed);
  } else if (cr->row == 0 && cr->col == 0) {
    failed = insert_at_table_start(fe);
    GOTO_END(failed);
  }

  size_t place_to_edit_offset = 0; // NOTE: offset in the fully built text
  for ( size_t i = 0; i < cr->row; i++) {
    // NOTE: no NULL-check needed for 'll' cause the loop 
    // would be unreachable, since cr->row would be 0
    place_to_edit_offset += ll->items[i] + 1; // NOTE: '+1' is for '\n' 
  }
  place_to_edit_offset += cr->col;

  for (size_t i = 0, pieces_len = 0; i < table->len; i++) {
    pieces_len += table->items[i].len;
    if (place_to_edit_offset == pieces_len) {
      failed = insert_after_piece(fe, i);
      GOTO_END(failed);
    } else if (pieces_len > place_to_edit_offset) {
      size_t edit_offset = table->items[i].len - (pieces_len - place_to_edit_offset);
      failed = insert_inside_piece(fe, i, edit_offset);
      GOTO_END(failed);
    }
  }

end:
  // FIXME: end lable should only handle clean-up logic
  if (!failed) {
    if (text_char == '\n'){
      fe->cursor.row++;
      fe->cursor.col = 0; 
    } else {
      fe->cursor.col++; 
    }
    fe->last_edit.cursor = fe->cursor;
    fe->last_edit.action = ACT_INSERT;
  }
  return failed;
#undef LAST_ACT_WAS_INSERT
#undef AT_LAST_LINE_END
}


void delete_at_piece_end(PieceTable* table, size_t piece_idx)
{
  Piece* p = &table->items[piece_idx];
  if (p->len - 1 > 0){
    p->len--;
  } else {
    // NOTE: otherwise garbage might get copied over
    if (table->len > 1) memmove(p, p + 1, (table->len - (piece_idx + 1)) * sizeof(*p));
    if (table->len) table->len--;
  }
}

bool delete_inside_piece(PieceTable* table, size_t piece_idx, size_t char_offset_in_piece)
{
  bool failed = 0;
  // FIXME+CHECK: i think char_offset HAS to be less than p->len, otherwise
  // it would point to some other piece's character. In that case, 
  // should it point to the next piece?
  DA_MAYBE_GROW(table, 1, PIECE_TABLE_INIT_CAP, PieceTable);
  Piece* p = &table->items[piece_idx];
  memmove(p + 2, p + 1, (table->len - (piece_idx + 1)) * sizeof(*p));
  p[1] = (Piece){
    p->which_buf, 
    p->offset + char_offset_in_piece + 1,
    p->len - (char_offset_in_piece + 1)
  };
  p->len = char_offset_in_piece;
  table->len++;
end:
  return failed;
}


bool FRED_delete_text(FredEditor* fe)
{
#define buf(p, offset)((!(p)->which_buf ? fe->file_buf.text: fe->add_buf.items)[(offset)])
#define AT_LAST_LINE_END (cr->row == ll->len - 1 && cr->col == (size_t)ll->items[cr->row])

  bool failed = 0;

  PieceTable* table = &fe->piece_table;
  LinesLen* ll = &fe->lines_len;
  Cursor* cr = &fe->cursor;

  if (table->len == 0 || (cr->row == 0 && cr->col == 0)) {
    return failed;
  }
  
  char del_char = 0;

  // NOTE+FIXME: this is not reached if you're editing 
  // a file that ends with EOL since the EOL is 
  // considered a proper line by get_lines_len()
  if (AT_LAST_LINE_END) {
    Piece* p = &table->items[table->len - 1];
    del_char = buf(p, p->offset + (p->len - 1));
    delete_at_piece_end(table, table->len - 1);
    GOTO_END(0);
  }

  size_t place_to_edit_offset = 0;
  for (size_t i = 0; i < cr->row; i++) {
    place_to_edit_offset += ll->items[i] + 1; 
  }
  place_to_edit_offset += cr->col; // NOTE: col is on the char after
  
  for (size_t i = 0, pieces_len = 0; i < table->len; i++) {
    Piece* p = &table->items[i];
    pieces_len += p->len;

    if (pieces_len == place_to_edit_offset) {
      del_char = buf(p, p->offset + p->len - 1);
      delete_at_piece_end(table, i);
      GOTO_END(0);

    } else if (pieces_len > place_to_edit_offset){
      size_t char_offset_in_piece =  p->len - 1 - (pieces_len - place_to_edit_offset);
      del_char = buf(p, p->offset + char_offset_in_piece);
      failed = delete_inside_piece(table, i, char_offset_in_piece);
      GOTO_END(failed);
    }
  }
end:
  if (!failed) {
    if (del_char == '\n') {
      if (cr->row) cr->row--;
      cr->col = ll->items[cr->row];
    } else {
      if (cr->col) cr->col--;
    }
    fe->last_edit.cursor = fe->cursor;
    fe->last_edit.action = ACT_DELETE;
  }
  return failed;
#undef buf
#undef AT_LAST_LINE_END
}



void dump_piece_table(FredEditor* fe, FILE* stream)
{

  fprintf(stream, "LINES-LENGHTS:\n");
  fprintf(stream, "arr-len: %ld\n", fe->lines_len.len );
  for (size_t i = 0; i < fe->lines_len.len; i++){
    fprintf(stream, "[%ld] = %d,\n", i + 1, fe->lines_len.items[i]);
  }

  fprintf(stream, "TABLE (len: %ld, cap: %ld):\n", fe->piece_table.len, fe->piece_table.cap);
  for (size_t i = 0; i < fe->piece_table.len; i++){
    Piece piece = fe->piece_table.items[i];
    char* buf = (!piece.which_buf ? fe->file_buf.text : fe->add_buf.items) + piece.offset;
    fprintf(stream, "[%ld] => [buf = %d, offset = %ld, len = %ld]:\n", 
            i, piece.which_buf, piece.offset, piece.len);

    fprintf(stream, "%.*s\n", (int)piece.len, buf);
    fprintf(stream, "----------------------------------------------------------------------\n");
  }
  fprintf(stream, "ADD-BUF:\n");
  fprintf(stream, "%.*s\n", (int)fe->add_buf.len, fe->add_buf.items);
  fprintf(stream, "----------------------------------------------------------------------\n");
}




// TODO+NOTE: EOL at end of file (like the ones saved 
// with neovim) will get considered as proper line 
bool FRED_get_lines_len(FredEditor* fe)
{
#define is_last_char(t, p, i, j) ((i) == (t)->len - 1 && (j) == (p)->len - 1)

  bool failed = 0;
  PieceTable* t = &fe->piece_table;
  LinesLen* ll = &fe->lines_len;
  
  ll->len = 0;
  size_t line_start = 0;
  size_t tot_text_len = 0;
  DA_PUSH(ll, 0, 8, LinesLen);

  for (size_t i = 0; i < t->len; i++) {
    Piece* p = &t->items[i];
    char* buf = !p->which_buf ? fe->file_buf.text : fe->add_buf.items;
    for (size_t j = 0; j < p->len; j++) {
      char c = buf[p->offset + j];
      if (c == '\n' || is_last_char(t, p, i, j)) {
        size_t line_end = tot_text_len + j + (c != '\n');
        size_t line_len = line_end - line_start;
        // TODO: what if file is data not separated by newlines?
        assert(line_len <= UINT16_MAX, "line-length overflow (max line-length is UINT16_MAX, 65535), "
                                       "length cannot be stored for later usage");
        ll->items[ll->len - 1] = (uint16_t)line_len;
        line_start = line_end + 1;
        if (c == '\n') DA_PUSH(ll, 0, 8, LinesLen);
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
    size_t rows = ll->items[tw->lines_to_scroll++] / tw_row_w + 1; // first line on the screen 
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





bool FRED_handle_input(FredEditor* fe, bool* running, bool* insert, char* key, ssize_t bytes_read)
{
  bool failed = 0;

  fe->cursor.prev_row = fe->cursor.row;
  fe->cursor.prev_col = fe->cursor.col;

  if (*insert){
    if (KEY_IS(key, "\x1b") || KEY_IS(key, "\x1b ")){ // escape
      fe->last_edit.cursor = fe->cursor;
      *insert = false;
    } else if (KEY_IS(key, "\x7f")){
      if (FRED_delete_text(fe)) GOTO_END(1);
    } else if (bytes_read == 1) {
      if (FRED_insert_text(fe, key[0])) GOTO_END(1);
    }
    FRED_get_lines_len(fe);
  } else {
    if (KEY_IS(key, "h") || KEY_IS(key, "j") || KEY_IS(key, "k") || KEY_IS(key, "l"))  {
      FRED_move_cursor(fe, key[0]);
    } else if (KEY_IS(key, "q")) {
      *running = false;
    } else if (KEY_IS(key, "i")) {
      *insert = true;
    } else if (KEY_IS(key, "\x1b") || KEY_IS(key, "\x1b ")) {
      *insert = false;
    }
  }
end:
  return failed;
}

bool FRED_start_editor(FredEditor* fe, const char* file_path)
{
  bool failed = 0;
  bool running = true;
  bool insert = false;

  TermWin tw = {0};
  tw.line_num_w = 8;
  if (FRED_win_resize(&tw)) GOTO_END(1);

  FRED_get_lines_len(fe);

#if 1
  while (running) {
    FRED_get_text_to_render(fe, &tw, insert);
    FRED_render_text(&tw, &fe->cursor);

    char key[MAX_KEY_LEN] = {0};
    ssize_t bytes_read = read(STDIN_FILENO, key, MAX_KEY_LEN);

    if (bytes_read == -1) {
      if (errno == EINTR){
        if (FRED_win_resize(&tw)) GOTO_END(1);
        update_win_cursor(fe, &tw);
        continue;
      }
      ERROR("failed to read from stdin");
    }

    if (bytes_read > 0) {
      if (FRED_handle_input(fe, &running, &insert, key, bytes_read)) GOTO_END(1);
      update_win_cursor(fe, &tw);
    }
  }
#endif 
  GOTO_END(failed);
end:
  if (!failed) { // NOTE: else the ERROR() macro has already cleared the screen
    fprintf(stdout, "\033[2J\033[H");
  }
  // dump_piece_table(fe, stdout);
  fred_editor_free(fe);
  free(tw.text);
  return failed;
}
