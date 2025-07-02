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
  PieceTable* table = &fe->piece_table;

  for (size_t i = 0; i < table->len; i++){
    file_size += table->items[i].len;
  }

  FILE* f = fopen(file_path, "wb");
  if (f == NULL){
    ERROR("failed to open '%s' while trying to save it. %s.", file_path, strerror(errno));
  }

  if (file_size > 0){
    for (size_t i = 0; i < table->len; i++){
      Piece p = table->items[i];
      // TODO: maybe we can just use the buf macro 
      char* buf = !p.which_buf ? fe->file_buf.text : fe->add_buf.items;
      fwrite(buf + p.offset, sizeof(*buf), p.len, f);
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
  DA_FREE(&fe->piece_table, 1);
  DA_FREE(&fe->add_buf, 1);
  DA_FREE(&fe->lines_len, 1);
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
  tw->height = w.ws_row;
  tw->width = w.ws_col;

  // void* temp = realloc(tw->elems, tw->size);
  // if (temp == NULL) ERROR("not enough memory to get and display text.");
  // tw->elems = temp;
  GOTO_END(failed);
end:
  return failed;
}


// TODO+NOTE: EOL at end of file (like the ones saved 
// with neovim) will get considered as proper line 
// NOTE: last 2 bytes in line-item are reserved 
// for the keywords-per-line count, used for 
// rendering highlighting
bool FRED_get_lines_len(FredEditor* fe)
{
#define is_last_char(t, p, i, j) ((i) == (t)->len - 1 && (j) == (p).len - 1)
#define buf(p, offset)((!(p).which_buf ? fe->file_buf.text: fe->add_buf.items)[(offset)])

  bool failed = 0;
  PieceTable* table = &fe->piece_table;
  LinesLen* ll = &fe->lines_len;
  
  ll->len = 0;
  size_t line_start = 0;
  size_t tot_text_len = 0;
  DA_PUSH(ll, 0, 8, LinesLen);

  for (size_t i = 0; i < table->len; i++) {
    Piece p = table->items[i];
    for (size_t j = 0; j < p.len; j++) {
      char c = buf(p, p.offset + j);
      if (c == '\n' || is_last_char(table, p, i, j)) {
        size_t line_end = tot_text_len + j + (c != '\n');
        size_t line_len = line_end - line_start;
        // TODO: what if file is some big ass data not separated by newlines?
        assert(line_len <= UINT16_MAX, "line-length overflow (max line-length is UINT16_MAX, 65535), "
                                       "length cannot be stored for later usage");
        ll->items[ll->len - 1] = (uint16_t)line_len;
        line_start = line_end + 1;
        if (c == '\n') DA_PUSH(ll, 0, 8, LinesLen);
      }
    }
    tot_text_len += p.len;
  }
end:
  return failed;
#undef is_last_char
#undef buf
}



// NOTE+TODO: get_lines_len() and build_table_text_for_render()
// parse the table twice back to back, so it would 
// be reasonable to have just one big function.
// However, this mixes up the editor's logic with 
// rendering logic, which doesn't play nice with 
// 'test.c' since it should only test the piece-table 
// (and later on the more of editor logic) and shouldn't 
// need any rendering info (TermWin array, win size etc.) at all.
// I tried using one big function and disabling 
// the rendering logic with the preprocessor,
// but i couldn't make it work and everything got messy.
// I would like to fix this in the future, but 
// for now we will keep it like this.


#if 0
// DESC: stores table-text in dyn-array, where
// each keyword's start is flagged with a negative keywordID.
// Also stores the keywords-per-line count in the last 
// 2 bytes of each LinesLen item. 
// All this is solely for easier rendering, 
// never used in editing logic.
bool build_table_text_for_render(FredEditor* fe, TermWin* tw)
{

#define highlight(keyword, keyword_len, keyword_id) do { \
  DA_MAYBE_GROW(tt, 1, TABLE_TEXT_INIT_CAP, TableText); \
  tt->items[tt->len - (keyword_len)] = (int8_t)(keyword_id) * -1; \
  memcpy(tt->items + tt->len - (keyword_len) + 1, (keyword), (keyword_len) * sizeof(*tt->items)); \
  tt->len += 1; \
  keywords_per_line++; \
} while (0)
#define match(match)(0 == memcmp(word, (match), word_len))
#define is_last_char(t, p, i, j) ((i) == (t)->len - 1 && (j) == (p).len - 1)
#define buf(p, offset) ((!(p).which_buf ? fe->file_buf.text: fe->add_buf.items)[(offset)])
#define MAX_WORD_LEN 32

  bool failed = 0;
  PieceTable* table = &fe->piece_table;
  LinesLen* ll = &fe->lines_len;
  size_t curr_line = 0;

  TableText* tt = &tw->table_text;
  tt->len = 0;  

  char word[MAX_WORD_LEN] = {0};
  size_t word_len = 0;
  size_t word_offset = 0;
  size_t keywords_per_line = 0;
  bool is_comment = false;

  for (size_t i = 0; i < table->len; i++) {
    Piece p = table->items[i];
    for (size_t j = 0; j < p.len; j++) {
      char c = buf(p, p.offset + j);

      if (c == '\n' || is_last_char(table, p, i, j)) {
        assert(curr_line < ll->len, "tried to write past limit, when saving keywords count per line");
        ll->items[curr_line++] |= (keywords_per_line << (16*1));
        keywords_per_line = 0;
      }

      #if 0
      if (word[0] == '/' && word[1] == '/') {
        highlight("//", word_len, KW_COMMENT);
        is_comment = true;
        word_len = 0;
        memset(word, 0, MAX_WORD_LEN);
      }

      if (!is_comment) {
        if (word_len >= MAX_WORD_LEN) {
          memset(word, 0, MAX_WORD_LEN);
          word_len = 0;

        } else if ((c < 'a' || c > 'z') && c != '#' && c != '/') {
          switch (word_len) {
            case 2: {
              if (word[0] == 'i' && word[1] == 'f') 
                highlight("if", word_len, KW_IF);
              break;
            }
            case 3: {
              if (word[0] == 'f' && match("for")) 
                highlight("for", word_len, KW_FOR);
              else if (word[0] == '#' && match("#if")) 
                highlight("#if", word_len, KW_IF_PREPROC);
              break;
            }
            case 4: {
              if (word[0] == 'e' && match("else")) 
                highlight("else", word_len, KW_ELSE);
              break;
            }
            case 5: {
              if (word[0] == 'w' && match("while")) 
                highlight("while", word_len, KW_WHILE);
              else if (word[0] == '#' && match("#else")) 
                highlight("#else", word_len, KW_ELSE_PREPROC);
              break;
            }
            case 6: {
              if (word[0] == 'r' && match("return")) 
                highlight("return", word_len, KW_RETURN);
              else if (word[0] == '#' && match("#ifdef")) 
                highlight("#ifdef", word_len, KW_IFDEF);
              else if (match("#endif")) 
                highlight("#endif", word_len, KW_ENDIF);
              break;
            }
            case 7: {
              if (word[1] == 'd' && match("#define")) 
                highlight("#define", word_len, KW_DEFINE);
              else if (word[1] == 'i' && match( "#ifndef")) 
                highlight("#ifndef", word_len, KW_IFNDEF);
              break;
            }
            case 8: {
              if (word[0] == 'c' && match("continue")) 
                highlight("continue", word_len, KW_CONTINUE);
              else if (word[0] == '#' && match( "#include")) 
                highlight("#include", word_len, KW_INCLUDE);
            }
          }
          memset(word, 0, MAX_WORD_LEN);
          word_len = 0;
        } else {
          word[word_len++] = c;
        }
      }

      if (c == '\n') is_comment = false;
      #endif 

      
      DA_PUSH(tt, c, TABLE_TEXT_INIT_CAP, TableText);
    }
    word_offset += p.len;
  }

end:
  return failed;
#undef buf
#undef matches
#undef MAX_WORD_LEN
}
#endif 


#if 0
// DESC: places the editor's text char-by-char
// into TermWin array, saves ID and row/col in TermWin
// of keywords into a dyn-array for highlighting. 
// When parsing a comment also saves 
// comment's length + any right/left-padding.
// This way we can print the highlighted comment directly
// from the TermWin array.
bool FRED_get_text_to_render(FredEditor* fe, TermWin* tw, bool insert)
{
#define buf(p, offset)((!(p).which_buf ? fe->file_buf.text: fe->add_buf.items)[(offset)])

  bool failed = 0;

  memset(tw->elems, SPACE_CH, tw->size);

  LinesLen* ll = &fe->lines_len;
  Cursor* cr = &fe->cursor;
  TableText* tt = &tw->table_text;
  HighlightOffsets* ho = &tw->ho;
  ho->len = 0;

  size_t last_row_offset = tw->size - tw->width;
  {
    size_t curs_offset = last_row_offset + tw->width - 1;
    size_t first_linenum_offset = tw->linenum_width - tw->linenum_width / 3;
    char* mode = insert ? "-- INSERT --" : "-- NORMAL --";
    memcpy(tw->elems + last_row_offset + 2, mode, strlen(mode));
    TW_WRITE_NUM_AT(tw, curs_offset, "%-d:%-d", (int)cr->row + 1, (int)cr->col + 1); 
    TW_WRITE_NUM_AT(tw, first_linenum_offset, "%ld", tw->lines_to_scroll + 1);
  }

  if (ll->len == 0) return failed;

  // TODO: cache it
  size_t fl_offset = 0; // First Line to render
  for (size_t i = 0; i < tw->lines_to_scroll; i++) {
    size_t item = ll->items[i];
    size_t line_len = (item & 0xffff) + ((item >> (16*1)) & 0xffff); // NOTE: line len + keywords count in line
    fl_offset +=  line_len + 1; // NOTE: '+1' is for '\n' 
  }

  size_t tw_elems_idx = tw->linenum_width;
  size_t line = tw->lines_to_scroll;
  size_t linenum_offset = tw->linenum_width / 3; // TODO: cache it 
  size_t tw_row = 0, tw_col = tw->linenum_width;
  size_t comment_start = 0;

  for (size_t i = fl_offset; i < tt->len; i++) {
    if (tw_elems_idx >= last_row_offset) break;
    char c = tt->items[i];

    if (c < 0) { // NOTE: Next there's a keyword to highlight
      size_t keyword_id = c * -1;
      if (keyword_id == KW_COMMENT) comment_start = tw_elems_idx;
      size_t item = tw_row | (tw_col << (16*1)) | (keyword_id << (16 * 2));
      DA_PUSH(ho, item, 8, HighlightOffsets); // TODO: make a ho-init-cap
      continue;
    }

    if (c != '\n'){ 
      if (tw_col + 1 > tw->width) {
        tw_elems_idx += tw->linenum_width;
        tw_col = tw->linenum_width;
        tw_row++;
      }
      if (comment_start && i == tt->len - 1) { // NOTE: saving comment length + any right/left-padding
        ho->items[ho->len - 1] |= ((tw_elems_idx + 1) - comment_start) << (16 * 3);
        comment_start = 0;
      }
      tw->elems[tw_elems_idx++] = c;
      tw_col++;
    } else {
      line++;
      if (comment_start) { // NOTE: saving comment length + any right/left-padding
        ho->items[ho->len - 1] |= (tw_elems_idx - comment_start) << (16 * 3);
        comment_start = 0;
      }
      tw_elems_idx += (tw->width - tw_col) + tw->linenum_width;
      tw_col = tw->linenum_width;
      tw_row++;
      if (tw_elems_idx < last_row_offset){
        TW_WRITE_NUM_AT(tw, tw_elems_idx - linenum_offset, "%ld", line + 1);
      }
    }
  }
end:
  return failed;
 
#undef buf
}
#endif



bool build_table_text(FredEditor* fe, TermWin* tw)
{
#define MAX_WORD_LEN 32
#define buf(p, _offset) ((!(p).which_buf ? fe->file_buf.text : fe->add_buf.items)[(p).offset + (_offset)])
#define highlight(tt, word, word_len) do { \
  DA_MAYBE_GROW((tt), 9, TABLE_TEXT_INIT_CAP, TableText); \
  memcpy((tt)->items + (tt)->len - (word_len), "\x1b[31m" word "\x1b[0m", ((word_len) + 9) * sizeof(*(tt)->items)); \
  (tt)->len += 9; \
} while(0)

  bool failed = 0;
  PieceTable* table = &fe->piece_table;
  TableText* tt = &tw->table_text;
  LinesLen* ll = &fe->lines_len;
  HighlightOffsets* ho = &tw->ho;
  tt->len = 0;
  ho->len = 0;

  char word[MAX_WORD_LEN] = {0};
  size_t word_len = 0;
  uint16_t row = 0;
  size_t keywords_per_line = 0;

  for (size_t i = 0; i < table->len; i++) {
    Piece p = table->items[i];
    for (size_t j = 0; j < p.len; j++){
      char c = buf(p, j);

      if (word_len >= MAX_WORD_LEN) {
        memset(word, 0, MAX_WORD_LEN);
        word_len = 0;
      } else if (c >= 'a' && c <= 'z') {
        word[word_len++] = c;
      } else {
        if (word_len == 2 && 0 == memcmp(word, "if", word_len)) {
          highlight(tt, "if", 2);
          keywords_per_line++;
        }
        memset(word, 0, MAX_WORD_LEN);
        word_len = 0;
      }
      DA_PUSH(tt, c, TABLE_TEXT_INIT_CAP, TableText);

      if (c == '\n'){
        ll->items[row] |= ((size_t)(ll->items[row] + (keywords_per_line)* 9) << 16);
        row++;
        keywords_per_line = 0;
      }
    }
  }
end:
  return failed;
}




bool render(FredEditor* fe, TermWin* tw, bool insert)
{
  bool failed = 0;
  LinesLen* ll = &fe->lines_len;
  TableText* tt = &tw->table_text;
  Cursor* cr = &fe->cursor;

  size_t fl_offset = 0;
  for (size_t i = 0; i < tw->lines_to_scroll; i++) {
    size_t item = ll->items[i];
    size_t tot_line_len = (item >> 16) & 0xffff;
    fl_offset += tot_line_len + 1; // NOTE: '+1' is for '\n' 
  }

  fprintf(stdout, "\x1b[2J\x1b[H");
  int linenum_offset = tw->linenum_width / 3;
  int row = 1, col = tw->linenum_width;
  signed char* line_start = &tt->items[fl_offset];

  for (size_t i = tw->lines_to_scroll; i < ll->len; i++){
    if ((size_t)row >= tw->height - 1) break;
    size_t item = ll->items[i];
    size_t tot_line_len = (item >> 16) & 0xffff;

    fprintf(stdout, 
            "\x1b[%d;%dH%d"
            "\x1b[%d;%dH",
            row, 1, (int)i + 1, // write linenum
            row, col);
    fprintf(stdout, "%.*s", (int)tot_line_len, line_start);

    line_start += tot_line_len + 1;
    row++;
    col = tw->linenum_width;
  }

  fprintf(stdout, "\x1b[%ld;%ldH", cr->win_row + 1, cr->win_col + tw->linenum_width);
  fflush(stdout);
end:
  return failed;
#undef buf
}





bool FRED_render_text(TermWin* tw, Cursor* cr)
{
  bool failed = 0;
  fprintf(stdout, "\x1b[H");
  fwrite(tw->elems, sizeof(*tw->elems), tw->size, stdout);
  
  HighlightOffsets* ho = &tw->ho;
  for (size_t i = 0; i < ho->len; i++) {
    size_t n = ho->items[i];
    uint16_t tw_row = n & 0xffff;
    uint16_t tw_col = (n >> (16 * 1)) & 0xffff;
    uint16_t kw_id = (n >> (16 * 2)) & 0xffff;
    uint16_t kw_len = 0;
    char* kw = NULL;

    switch (kw_id) {
      case KW_IF:           { kw_len = 2; kw = "if"; break; }
      case KW_WHILE:        { kw_len = 5; kw = "while"; break; }
      case KW_RETURN:       { kw_len = 6; kw = "return"; break; }
      case KW_FOR:          { kw_len = 3; kw = "for"; break; }
      case KW_CONTINUE:     { kw_len = 8; kw = "continue"; break; }
      case KW_ELSE:         { kw_len = 4; kw = "else"; break; }
      case KW_INCLUDE:      { kw_len = 8; kw = "#include"; break; }
      case KW_IFDEF:        { kw_len = 6; kw = "#ifdef"; break; }
      case KW_IFNDEF:       { kw_len = 7; kw = "#ifndef"; break; }
      case KW_ELSE_PREPROC: { kw_len = 5; kw = "#else"; break; }
      case KW_IF_PREPROC:   { kw_len = 3; kw = "#if"; break; }
      case KW_ENDIF:        { kw_len = 6; kw = "#endif"; break; }
      case KW_DEFINE:       { kw_len = 7; kw = "#define"; break; }
      case KW_COMMENT: {
        uint16_t comment_len = (n >> (16 * 3) & 0xffff);
        char* comment_start = &tw->elems[tw_row * tw->width + tw_col];
        fprintf(stdout, "\x1b[%u;%uH", tw_row + 1, tw_col + 1);
        fprintf(stdout, "\x1b[33m%.*s\x1b[0m", (int)(comment_len), comment_start);
        continue; // NOTE: text is already wrapped 
      }
      default: { 
        fflush(stdout);
        ERROR("internal (highlighting): unexpected keyword ID (%d)", kw_id);
      }
    }

    fprintf(stdout, "\x1b[%u;%uH", tw_row + 1, tw_col + 1);
    if (tw_col + kw_len > tw->width) { // keywords wraps into the next line
      size_t kw_rest = tw_col + kw_len - tw->width;
      fprintf(stdout, "\x1b[31m%.*s", (int)(kw_len - kw_rest), kw);
      fprintf(stdout, "\x1b[%u;%uH", tw_row + 2, tw->linenum_width + 1);
      fprintf(stdout, "%.*s\x1b[0m", (int)kw_rest, kw + (kw_len - kw_rest));
    } else {
      fprintf(stdout, "\x1b[31m%.*s\x1b[0m",(int)kw_len, kw);
    }
  }

  fprintf(stdout, "\x1b[%zu;%zuH", cr->win_row + 1, tw->linenum_width + cr->win_col + 1);
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


// DESC: either grows the piece at 'piece_idx' or 
// adds a new piece after it
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





// DESC: splits the piece at 'piece_idx' 
// into two and adds a new one in the middle 
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
#define AT_LAST_LINE_END (cr->row == ll->len - 1 && cr->col == (size_t)(ll->items[cr->row] & 0xffff))
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
    place_to_edit_offset += (ll->items[i] & 0xffff) + 1; // NOTE: '+1' is for '\n' 
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
      cr->row++;
      cr->col = 0; 
    } else {
      cr->col++; 
    }
    fe->last_edit.cursor = *cr;
    fe->last_edit.action = ACT_INSERT;
  }
  return failed;
#undef LAST_ACT_WAS_INSERT
#undef AT_LAST_LINE_END
}




// DESC: either shrinks the piece at 'piece_idx'
// or deletes it by moving all the next pieces on top of it  
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
#define buf(p, offset)((!(p).which_buf ? fe->file_buf.text: fe->add_buf.items)[(offset)])
#define AT_LAST_LINE_END (cr->row == ll->len - 1 && cr->col == (size_t)(ll->items[cr->row] & 0xffff))

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
    Piece p = table->items[table->len - 1];
    del_char = buf(p, p.offset + (p.len - 1));
    delete_at_piece_end(table, table->len - 1);
    GOTO_END(0);
  }

  size_t place_to_edit_offset = 0;
  for (size_t i = 0; i < cr->row; i++) {
    place_to_edit_offset += (ll->items[i] & 0xffff) + 1; 
  }
  place_to_edit_offset += cr->col; // NOTE: col is on the char after
  
  for (size_t i = 0, tot_pieces_len = 0; i < table->len; i++) {
    Piece p = table->items[i];
    tot_pieces_len += p.len;

    if (tot_pieces_len == place_to_edit_offset) {
      del_char = buf(p, p.offset + p.len - 1);
      delete_at_piece_end(table, i);
      GOTO_END(0);

    } else if (tot_pieces_len > place_to_edit_offset){
      size_t char_offset_in_piece =  p.len - 1 - (tot_pieces_len - place_to_edit_offset);
      del_char = buf(p, p.offset + char_offset_in_piece);
      failed = delete_inside_piece(table, i, char_offset_in_piece);
      GOTO_END(failed);
    }
  }
end:
  if (!failed) {
    if (del_char == '\n') {
      if (cr->row) cr->row--;
      cr->col = (ll->items[cr->row] & 0xffff);
    } else {
      if (cr->col) cr->col--;
    }
    fe->last_edit.cursor = *cr;
    fe->last_edit.action = ACT_DELETE;
  }
  return failed;
#undef buf
#undef AT_LAST_LINE_END
}



void dump_piece_table(FredEditor* fe, FILE* stream)
{
  // TODO: this shits ass make it better
  fprintf(stream, "LINES-LENGHTS:\n");
  fprintf(stream, "arr-len: %ld\n", fe->lines_len.len );
  for (size_t i = 0; i < fe->lines_len.len; i++){
    fprintf(stream, "[%ld] = %d,\n", i + 1, (fe->lines_len.items[i] & 0xffff));
  }

#if 0
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
#endif
}






void FRED_move_cursor(FredEditor* fe, char key) 
{
  Cursor* cr = &fe->cursor;
  cr->prev_row = cr->row;
  cr->prev_col = cr->col;
  size_t tot_lines = fe->lines_len.len;

  // TODO: store curr line length in cursor 

  if (!tot_lines) return;

  switch (key){
    case 'h': {
      if ((int)cr->col - 1 < 0) return;
      cr->col--;
      return;
    } 
    case 'l': {
      size_t curr_line_len = tot_lines == 0 ? 0 : (fe->lines_len.items[cr->row] & 0xffff);
      if (cr->col + 1 > curr_line_len) return;
      cr->col++;
      return;
    }
    case 'j': {
      if (cr->row >= tot_lines - 1) return;
      cr->row++;
      size_t line_len = fe->lines_len.items[cr->row] & 0xffff;
      if (cr->col > line_len) {
        cr->col = line_len;
      }
      return;
    }
    case 'k': {
      if ((int64_t)cr->row - 1 < 0) return;
      cr->row--;
      size_t line_len = fe->lines_len.items[cr->row] & 0xffff;
      if (cr->col > line_len) {
        cr->col = line_len;
      }
      return;
    }
  }
}




// FIXME: broken on small resized win + what the fuck 
void update_win_cursor(FredEditor* fe, TermWin* tw)
{
  LinesLen* ll = &fe->lines_len;
  if (ll->items == NULL || !ll->len) return;

  Cursor* cr = &fe->cursor;
  size_t tw_row_w = tw->width - tw->linenum_width;

  size_t win_col = cr->col % tw_row_w;
  cr->win_col = win_col;

  size_t mid = tw->height * 0.5;

  if (cr->win_row > mid + 5) {
    size_t curr_line_rows = (ll->items[cr->row] & 0xffff) / tw_row_w + 1;
    size_t rows = (ll->items[tw->lines_to_scroll++] & 0xffff) / tw_row_w + 1; // first line on the screen 
    // NOTE: the 2nd check will render the current line closer the center if it's wrapped
    while (rows < curr_line_rows || (curr_line_rows > 1 && rows <= curr_line_rows)) {
      rows += (ll->items[++tw->lines_to_scroll] & 0xffff) / tw_row_w + 1;
    }
  } else if (tw->lines_to_scroll && cr->win_row < mid - 5) {
    size_t prev_line_rows = (ll->items[cr->prev_row] & 0xffff) / tw_row_w + 1;
    size_t rows = (ll->items[--tw->lines_to_scroll] & 0xffff) / tw_row_w + 1;
    while (tw->lines_to_scroll && rows < prev_line_rows) {
      rows += (ll->items[--tw->lines_to_scroll] & 0xffff) / tw_row_w + 1;
    }
  }
  
  // NOTE: loops from the 1st line on the screen
  size_t win_row = 0;
  for (size_t i = tw->lines_to_scroll; i < cr->row; i++) {
    size_t line_len = (ll->items[i] & 0xffff);
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
    if (FRED_get_lines_len(fe)) GOTO_END(1);
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

  // TODO: make a term_win_init();
  TermWin tw = {0};
  tw.table_text = (TableText){0};
  tw.ho = (HighlightOffsets){0};
  tw.linenum_width = 8;
  if (FRED_win_resize(&tw)) GOTO_END(1);

  if (FRED_get_lines_len(fe)) GOTO_END(1);
  if (build_table_text(fe, &tw)) GOTO_END(1);

  // if (render(fe, &tw, true)) GOTO_END(1);

  // if (build_table_text_for_render(fe, &tw)) GOTO_END(1);
  // if (FRED_get_text_to_render(fe, &tw, insert)) GOTO_END(1); 
  
#if 1
  while (running) {
    // if (FRED_render_text(&tw, &fe->cursor)) GOTO_END(1);
    if (render(fe, &tw, true)) GOTO_END(1);

    char key[MAX_KEY_LEN] = {0};
    ssize_t bytes_read = read(STDIN_FILENO, key, MAX_KEY_LEN);
    if (bytes_read == -1) {
      if (errno == EINTR){
        if (FRED_win_resize(&tw)) GOTO_END(1);
        // if (FRED_get_text_to_render(fe, &tw, insert)) GOTO_END(1); 
        if (build_table_text(fe, &tw)) GOTO_END(1);
        update_win_cursor(fe, &tw);
        continue;
      }
      ERROR("failed to read from stdin");
    }

    if (bytes_read > 0) {
      bool was_insert = insert;
      if (FRED_handle_input(fe, &running, &insert, key, bytes_read)) GOTO_END(1);
      update_win_cursor(fe, &tw);
      if (was_insert) {
        // if (build_table_text_for_render(fe, &tw)) GOTO_END(1);
        if (build_table_text(fe, &tw)) GOTO_END(1);
      }
      // if (FRED_get_text_to_render(fe, &tw, insert)) GOTO_END(1); 
    }
  }
#endif 
end:
  if (!failed) { // NOTE: else the ERROR() macro has already cleared the screen
    fprintf(stdout, "\x1b[2J\x1b[H");
  }
  // dump_piece_table(fe, stdout);
  fred_editor_free(fe);
  free(tw.elems);
  free(tw.table_text.items);
  free(tw.ho.items);
  return failed;
}

// Thanks for for  the main loop structure in FRED_start_editor(), 
// the idea in FRED_get_text_to_render() of storing 
// the to-be-rendered text in a buffer, the error handling 
// and more generally for the inspiration at the beginning of the project:
//
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
