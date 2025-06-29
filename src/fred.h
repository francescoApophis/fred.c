#ifndef FRED_H
#define FRED_H

#include "common.h"
#include <stdbool.h>
#include <stdio.h>



#define ADD_BUF_INIT_CAP 512
#define PIECE_TABLE_INIT_CAP 8 
#define TABLE_TEXT_INIT_CAP 512

#define SPACE_CH 32
#define ESC_CH 27
#define MAX_KEY_LEN 32



#define GOTO_END(value) do { failed = (value) ; goto end; } while (0)


#define ERROR(...) do { \
  fprintf(stderr, "\033[2J\033[H"); \
  fprintf(stderr, "ERROR: ") ; \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
  GOTO_END(1); \
} while (0)


#define PRINT(statement) do { \
  fprintf(stdout, "\033[2J\033[H"); \
  { \
    statement \
  } \
  exit(1); \
} while (0)



#define assert(cond, ...) do {                       \
  if (!(cond)){                                      \
    fprintf(stderr, "\033[2J\033[H");              \
    fprintf(stderr, "ASSERTION FAILED:" #cond "\n"); \
    fprintf(stderr, "%s, %d\n", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__);                    \
    fprintf(stderr, "\n");                           \
    GOTO_END(1); \
  }                                                  \
} while(0)

#define assert2(cond, ...) do {                       \
  if (!(cond)){                                      \
    fprintf(stderr, "\033[2J\033[H");              \
    fprintf(stderr, "ASSERTION FAILED:" #cond "\n"); \
    fprintf(stderr, "%s, %d\n", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__);                    \
    fprintf(stderr, "\n");                           \
    exit(1); \
  }                                                  \
} while(0)


#define DA_MAYBE_GROW(da, elem_count, da_cap_init, da_type) do {                      \
  if ((da)->len + (elem_count) > (da)->cap) {                                   \
    (da)->cap = (da)->cap == 0 ? (da_cap_init) : (da)->cap * 2;                     \
    void* temp = realloc((da)->items, (da)->cap * sizeof(*(da)->items));              \
    if (temp == NULL) ERROR("not enough memory for dynamic array \"" #da_type "\"."); \
    (da)->items = temp;                                                               \
  }                                                                                   \
} while(0)


#define DA_PUSH(da, item, da_cap_init, da_type) do {  \
  DA_MAYBE_GROW((da), 1, (da_cap_init), (da_type));   \
  (da)->items[(da)->len++] = (item);                  \
} while (0)


#define DA_INIT(da) do { \
  (da)->cap = 0;         \
  (da)->len = 0;         \
  (da)->items = NULL;    \
} while(0)

#define DA_FREE(da, end) do { \
  free((da)->items);          \
  if (!(end)){                \
    DA_INIT((da));            \
  }                           \
} while(0)

#define PIECE_TABLE_PUSH(da, item) do {                     \
  DA_PUSH((da), (item), PIECE_TABLE_INIT_CAP, PieceTable);  \
} while (0)


#define PIECE_TABLE_INSERT(piece_table, piece_idx, piece) do {  \
  (piece_table)->items[(piece_idx)] = (piece);                  \
  (piece_table)->len++;                                         \
} while(0)





#define ADD_BUF_PUSH(da, item) do {                 \
  DA_PUSH((da), (item), ADD_BUF_INIT_CAP, AddBuf);  \
} while (0)


#define KEY_IS(key, what) (strcmp((key), (what)) == 0)


#define TW_WRITE_NUM_AT(tw, offset, format, ...) do {                 \
  char num_digits = snprintf(NULL, 0, format, __VA_ARGS__);           \
  char num_str[num_digits + 1];                                           \
  sprintf(num_str, format, __VA_ARGS__);                              \
  memcpy((tw)->elems + ((offset) - num_digits), num_str, num_digits);  \
} while (0)


typedef struct termios termios;




typedef struct {
  bool which_buf;
  size_t offset;
  size_t len;
} Piece;

typedef struct {
  Piece* items;
  size_t len;
  size_t cap;
} PieceTable;


typedef struct {
  uint32_t* items; // NOTE: 1st 2 bytes -> actual line length; 2nd 2 bytes -> line length + keyword-byte-flags count 
  size_t len; // total lines in piece-table
  size_t cap;
} LinesLen;

typedef struct {
  size_t row; 
  size_t col; 
  size_t prev_row; 
  size_t prev_col; 
  size_t win_row;
  size_t win_col;
} Cursor;


// NOTE: an item contains the row, col in the 
// term-win and length of the keyword (present
// in the screen) that needs to be highlighted.
// All three of them are stored into 2 bytes, LSB order. 
typedef struct {
  size_t* items;
  size_t len;
  size_t cap;
} HighlightOffsets;

// NOTE: id are negative so it's safer and easier 
// to detect them in the TermWin char array.
typedef enum {
  KW_IF = 1,
  KW_ELSE,
  KW_RETURN,
  KW_CONTINUE,
  KW_WHILE,
  KW_FOR,
  KW_DEFINE,
  KW_INCLUDE,
  KW_IFDEF,
  KW_IFNDEF,
  KW_IF_PREPROC,
  KW_ELSE_PREPROC,
  KW_ENDIF,
  KW_COMMENT,
  KW_COUNT,
} KeywordId;




typedef struct {
  signed char* items; // negative char represents start of keyword, for highlighting 
  size_t len;
  size_t cap;
} TableText;  // NOTE: stores the fully built and highlighted 
              // text, only used for rendering


// TODO: only size and lines_to_scroll need to be size_t
typedef struct {
  char* elems; // stores the text put in the right place, ready to be rendered
  TableText table_text;
  HighlightOffsets ho;
  size_t size;
  size_t width;
  size_t height;
  size_t lines_to_scroll;
  short linenum_width; // NOTE: the max width between the left side of the screen 
                       // and the start of the text; for displaying line-nums
} TermWin;


typedef struct {
  char* text;
  size_t size;
} FileBuf;


typedef struct {
  char* items; // NOTE: array of strings because it should also handle multi-byte chars
  size_t len;
  size_t cap;
} AddBuf;


typedef enum {
  ACT_IDLE,
  ACT_INSERT,
  ACT_DELETE,
} Action;

typedef struct {
  Action action;
  Cursor cursor;
} LastEdit;


typedef struct {
  PieceTable piece_table;
  AddBuf add_buf;
  FileBuf file_buf;
  LinesLen lines_len;
  Cursor cursor;
  LastEdit last_edit;
} FredEditor;


bool FRED_open_file(FileBuf* file_buf, const char* file_path);
bool FRED_setup_terminal();
bool FRED_render_text(TermWin* tw, Cursor* cursor);
bool fred_editor_init(FredEditor* fe, const char* file_path);
void fred_editor_free(FredEditor* fe);
bool FRED_start_editor(FredEditor* fe, const char* file_path);
bool FRED_win_resize(TermWin* term_win);
bool FRED_get_text_to_render(FredEditor* fe, TermWin* term_win, bool insert);
bool FRED_insert_text(FredEditor* fe, char c);
void dump_piece_table(FredEditor* fe, FILE* stream);
void FRED_move_cursor(FredEditor* fe, char key);
bool FRED_delete_text(FredEditor* fe);
bool FRED_handle_input(FredEditor* fe, bool* running, bool* insert, char* key, ssize_t bytes_read);



#endif 
