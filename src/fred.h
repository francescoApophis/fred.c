#ifndef FRED_H
#define FRED_H

#include <stdbool.h>
#include <stdio.h>



#define ADD_BUF_INIT_CAP 512
#define PIECE_TABLE_INIT_CAP 59


#define SPACE_CH 32
#define ESC_CH 27



#define GOTO_END(value) do { failed = (value) ; goto end; } while (0)




#define ERROR(msg) do { \
  fprintf(stderr, "Error [in: %s, at line: %d]: %s (errno=%d)\n",  __FILE__, __LINE__, msg, errno); \
  GOTO_END(1); \
} while (0)



#define DA_PUSH(da, item, da_cap_init, da_type) do {                                  \
  if ((da)->len + 1 >= (da)->cap) {                                                   \
    (da)->cap = (da)->cap == 0 ? (da_cap_init) : (da)->cap * 2;                       \
    void* temp = realloc((da)->items, sizeof(*(da)->items) * (da)->cap);              \
    if (temp == NULL) ERROR("not enough memory for dynamic array \"" #da_type "\"."); \
    (da)->items = temp;                                                               \
  }                                                                                   \
  (da)->items[(da)->len++] = (item);                                                  \
} while (0)


#define DA_INIT(da) do { \
  (da)->cap = 0;                  \
  (da)->len = 0;                  \
  (da)->items = NULL;             \
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

#define ADD_BUF_PUSH(da, item) do { \
  DA_PUSH((da), (item), ADD_BUF_INIT_CAP, AddBuf);  \
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
} Cursor;


typedef struct {
  char* text;
  size_t size;
  size_t rows;
  size_t cols;
} TermWin;


typedef struct {
  char* text;
  size_t size;
} FileBuf;


typedef struct {
  char* items;
  size_t len;
  size_t cap;
} AddBuf;


typedef struct {
  PieceTable piece_table;
  AddBuf add_buf;
  FileBuf file_buf;
  Cursor cursor;
} FredEditor;


bool FRED_open_file(FileBuf* file_buf, const char* file_path);
bool FRED_open_file(FileBuf* file_buf, const char* file_path);
bool FRED_setup_terminal();
bool FRED_render_text(TermWin* term_win, Cursor* c);
bool fred_editor_init(FredEditor* fe, const char* file_path);
void fred_editor_free(FredEditor* fe);
bool FRED_start_editor(FredEditor* fe, const char* file_path);
bool fred_win_resize(TermWin* term_win);
void fred_get_text_from_piece_table(FredEditor* fe, TermWin* term_win, bool insert);
bool fred_make_piece(FredEditor* fe, char key);
void dump_piece_table(FredEditor* fe);


#endif 
