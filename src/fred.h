#ifndef FRED_H
#define FRED_H

#include <stdbool.h>
#include <stdio.h>



#define ADD_BUF_INIT_CAP 512
#define PIECE_TABLE_INIT_CAP 59

#define DA_PUSH(da, item, da_cap_init, da_type) do {                                  \
  if ((da)->len + 1 >= (da)->cap) {                                                   \
    (da)->cap = (da)->cap == 0 ? (da_cap_init) : (da)->cap * 2;                       \
    void* temp = realloc((da)->items, sizeof(*(da)->items) * (da)->cap);              \
    if (temp == NULL) ERROR("not enough memory for dynamic array \"" #da_type "\"."); \
    (da)->items = temp;                                                               \
  }                                                                                   \
  (da)->items[(da)->len++] = (item);                                                  \
} while (0)



#define PIECE_TABLE_INIT(da) do { \
  (da)->cap = 0;                  \
  (da)->len = 0;                  \
  (da)->items = NULL;             \
} while(0)

#define PIECE_TABLE_FREE(da) do { \
  free((da)->items);              \
  PIECE_TABLE_INIT((da));         \
} while(0)

#define PIECE_TABLE_PUSH(da, item) do {                     \
  DA_PUSH((da), (item), PIECE_TABLE_INIT_CAP, PieceTable);  \
} while (0)


typedef struct termios termios;

typedef struct {
  char* text;
  size_t size;
} FredFile;

// NOTE: using int instead of size_t could save much more memory 
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
  PieceTable pt;
  Cursor cursor;
} FredEditor;


bool FRED_open_file(FredFile* ff, const char* file_path);
bool FRED_setup_terminal(termios* term_orig);
bool FRED_render(bool idle, FredFile* ff);
bool FRED_start_editor(FredFile* ff);
bool fred_editor_init(FredEditor* fe, FredFile* ff); 

#endif 
