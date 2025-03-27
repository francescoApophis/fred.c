#ifndef FRED_H
#define FRED_H

#include "common.h"
#include <stdbool.h>
#include <stdio.h>



#define ADD_BUF_INIT_CAP 512
#define PIECE_TABLE_INIT_CAP 56

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


#define PRINTING(statement) do { \
  printing(); \
  { \
    statement \
  } \
  exit(1); \
} while (0)



#define ASSERT(cond) do {                                 \
  if (!(cond)){                                           \
    fprintf(stdout, "\033[2J\033[H\n");                   \
    fprintf(stderr, "ASSERTION FAILED:" #cond "\n");   \
    fprintf(stderr, "%s, %d\n", __FILE__, __LINE__);      \
    exit(1);                                              \
  }                                                       \
} while(0)

#define ASSERT_MSG(cond, format, ...) do {                \
  if (!(cond)){                                           \
    fprintf(stdout, "\033[2J\033[H\n");                   \
    fprintf(stderr, "ASSERTION FAILED: " #cond "\n");   \
    fprintf(stderr, "%s, %d\n", __FILE__, __LINE__);      \
    fprintf(stderr, format, __VA_ARGS__);                 \
    fprintf(stderr, "\n");                                \
    exit(1);                                              \
  }                                                       \
} while(0)


#define DA_MAYBE_GROW(da, elem_count, da_cap_init, da_type) do {                      \
  if ((da)->len + (elem_count) >= (da)->cap) {                                        \
    (da)->cap = (da)->cap == 0 ? (da_cap_init) : (da)->cap * 2;                       \
    void* temp = realloc((da)->items, sizeof(*(da)->items) * (da)->cap);              \
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




#define PIECE_TABLE_MAKE_ROOM(piece_table, piece_ptr, dest_offset, src_offset, n_bytes) do {            \
  if (!(n_bytes)) ASSERT_MSG(!(dest_offset) && !(src_offset), "%s", "Trying to 'memmove' 0 bytes." );   \
  DA_MAYBE_GROW((piece_table), (dest_offset) - (src_offset), PIECE_TABLE_INIT_CAP, PieceTable);         \
  if ((dest_offset) || (src_offset)) {                                                                  \
    memmove((piece_ptr) + (dest_offset), (piece_ptr) + (src_offset), (n_bytes) * sizeof(Piece));        \
  }                                                                                                     \
} while(0)


#define PIECE_TABLE_INSERT(piece_table, piece, wb, of, l) do {  \
  (piece) = ((Piece){.which_buf = wb, .offset = of, .len = l}); \
  (piece_table)->len++;                                         \
} while(0)


#define ADD_BUF_PUSH(da, item) do {                 \
  DA_PUSH((da), (item), ADD_BUF_INIT_CAP, AddBuf);  \
} while (0)


#define KEY_IS(key, what) (strcmp((key), (what)) == 0)


#define TW_WRITE_NUM_AT(tw, offset, format, ...) do {                 \
  char num_digits = snprintf(NULL, 0, format, __VA_ARGS__);           \
  char num_str[num_digits];                                           \
  sprintf(num_str, format, __VA_ARGS__);                              \
  memcpy((tw)->text + ((offset) - num_digits), num_str, num_digits);  \
} while (0)


typedef struct termios termios;



// NOTE: using uint32_t instead of size_t could save much more memory 
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
  size_t row; 
  size_t col; 
  size_t win_row;
  size_t win_col;
} Cursor;


typedef struct {
  char* text;
  size_t size;
  size_t rows;
  size_t cols;
  size_t lines_to_scroll;
  short line_num_w; // NOTE: the max width for displaying line-nums
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
  Action action;  // NOTE: 1 = insertion, 0 = deletion
  Cursor cursor;
} LastEdit;


typedef struct {
  PieceTable piece_table;
  AddBuf add_buf;
  FileBuf file_buf;
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
void FRED_get_text_to_render(FredEditor* fe, TermWin* term_win, bool insert);
bool FRED_insert_text(FredEditor* fe, char c);
bool FRED_make_piece(FredEditor* fe, bool which_buf, size_t offset, size_t len);
void dump_piece_table(FredEditor* fe);

#endif 
