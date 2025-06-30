#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>

#include "./../src/fred.h"


typedef struct {
  char* txt;
  size_t size;
} File;


char* test_dir_path = NULL; 
size_t keys_count = 0;
char* keys = NULL;
File snaps_file = {0};
size_t snaps_count = 0;
size_t* snaps_offsets = NULL; 
size_t* curs_coords = NULL; // FIXME: no need for it to be size_t



#define ERR(...) do { \
  fprintf(stderr, "ERROR: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
  exit(1); \
} while (0)

#define assert_(cond, ...) do { \
  if (!(cond)){ \
    fprintf(stderr, "[%s, line: %d] ASSERTION FAILED '" #cond "':\n", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
    exit(1); \
  } \
} while (0)




// DESC: makes a file-path from 'test_dir_path' and asserts it's valid 
char* make_path(const char* file_name)
{
  size_t n = strlen(test_dir_path);
  size_t m = strlen(file_name);
  bool has_sep = test_dir_path[n - 1] == '/';
  size_t full_path_len = n + m + (!has_sep ? 2 : 1);
  char* full_path = malloc(full_path_len * sizeof(*full_path));
  assert_(full_path != NULL, "not enough memory");

  memset(full_path, 0, full_path_len * sizeof(*full_path));

  strncat(full_path, test_dir_path, n * sizeof(*full_path));
  if (!has_sep) strcat(full_path, "/");
  strncat(full_path, file_name, m * sizeof(*full_path));

  struct stat sb;
  if (stat(full_path, &sb) == -1) {
    // TODO: print all files needed for the fred_tests
    if (errno == ENOENT) ERR("no such test-file '%s'.", full_path);
    ERR("failed to retrieve any info about test-file '%s', %s.", full_path, strerror(errno));
  }
  if ((sb.st_mode & S_IFMT) != S_IFREG){
    if ((sb.st_mode & S_IFMT) == S_IFDIR) ERR("given test-file-path '%s' is a directory, not a file.", full_path);
    ERR("given test-file-path '%s' is not a regular file.", full_path);
  }

  return full_path;
}


File read_file(const char* filename)
{
  FILE* file = fopen(filename, "rb");
  if (file == NULL){
    fprintf(stderr, "ERROR: could not open file '%s'.\n", filename);
    exit(1);
  }

  fseek(file, 0L, SEEK_END);
  long filesize = ftell(file);
  rewind(file);

  char* buffer = malloc(sizeof(*buffer) * (filesize + 1));
  assert_(buffer != NULL, "not enough memory");

  size_t n = fread(buffer, sizeof(char), filesize, file);
  if (n < filesize) {
    fprintf(stderr, "ERROR: could not read file '%s'.\n", filename);
    exit(1);
  }
  buffer[n] = '\0';
  fclose(file);

  File f = {.txt = buffer, .size = filesize};
  return f;
}


// DESC: convert content of keys.txt from ascii-int to chars and 
// save it in a buffer
char* get_keys(const char* file_name) 
{
  File file = read_file(file_name);

  for (size_t i = 0; i < file.size; i++){
    if (file.txt[i] == '\n') keys_count++;
  }
  keys = malloc(keys_count * sizeof(*keys));
  assert_(keys != NULL, "not enough memory");

  int idx = 0;
  char* curr = file.txt;
  char* end = curr;
  while (curr < file.txt + file.size) {
    keys[idx++] = (char)strtol(curr, &end, 10);
    curr = end + 1;
  }
  free(file.txt);
  return keys;
}
 
void print_key(FILE* stream, char c, bool end_newline) {
  fprintf(stream, "'");
  switch(c) {
    case 127: { fprintf(stream, "BACKSPACE"); break; }
    case 27:  { fprintf(stream, "ESC"); break; }
    case 10:  { fprintf(stream, "NEWLINE"); break; }
    case 9:   { fprintf(stream, "TAB"); break; }
    default:  { fprintf(stream, "%c", c); break; }
  }
  fprintf(stream, "'");
  fprintf(stream, " (ASCII %d)", c);
  if (end_newline) fprintf(stream, "\n");
}

void print_keys()
{
  for (size_t i = 0; i < keys_count; i++){
    print_key(stdout, keys[i], true);
  }
}


// DESC: gets and stores cursor coordinates from the end of a snapshot's label
// into a 1d array -> [snap1_row, snap1_col, snap2_row, snap2_col, ...].
// They can be easily accessed with the currently tested snap_num
size_t get_cursor_coords(size_t end_label_idx, size_t curs_coords_idx)
{
  int semicolon_idx = 0;
  while (snaps_file.txt[end_label_idx] != ' ') {
    if (snaps_file.txt[end_label_idx] == ':') semicolon_idx = end_label_idx;
    end_label_idx--;
  }
  int row = strtol(snaps_file.txt + end_label_idx, NULL, 10);
  int col = strtol(snaps_file.txt + semicolon_idx + 1, NULL, 10);
  assert_(curs_coords_idx < snaps_count * 2,
         "curs_cords_idx: %ld, max_curs_coords: %ld", curs_coords_idx, snaps_count * 2);
  curs_coords[curs_coords_idx] = row;
  curs_coords[curs_coords_idx + 1] = col;
  return curs_coords_idx + 2;
}

// DESC: gets and stores the snapshots offsets (start and length of a snapshot 
// in the file) into a 1d array -> [snap1_start, snap1_len, snap2_start, snap2_len, ...].
// They can be easily accessed with the currently tested 'snap_num'
void parse_snaps()
{
#define matches_label(ch) ((ch) == '\n' && 0 == strncmp(&(ch), label, label_len))

  const char* label = "\n[snapshot: ";
  const size_t label_len = strlen(label);
  
  for (int i = snaps_file.size; i >= 0 ; i--){
    if (matches_label(snaps_file.txt[i])){
      char* count_str = &snaps_file.txt[(size_t)i + label_len];
      snaps_count = (size_t)strtol(count_str, NULL, 10);
      break;
    }
  }

  size_t curs_coords_idx = 0;
  curs_coords = malloc(snaps_count * 2 * sizeof(*curs_coords));
  assert_(curs_coords != NULL, "not enough memory");

  size_t snaps_offsets_idx = 0;
  snaps_offsets = malloc(snaps_count * 2 * sizeof(*snaps_offsets));
  assert_(snaps_offsets != NULL, "not enough memory");

  size_t i = 0;
  size_t last_snap_start = 0; 
  bool parsing_label = false;
  while(i < snaps_file.size){
    if (parsing_label && snaps_file.txt[i] == '\n') { // reached label's end
      curs_coords_idx = get_cursor_coords(i, curs_coords_idx);
      parsing_label = false;
      last_snap_start = i + 1;
      snaps_offsets[snaps_offsets_idx++] = last_snap_start;
    }

    if (matches_label(snaps_file.txt[i]) || i + 1 >= snaps_file.size){
      parsing_label = true;
      if (i > 0) { 
        // saving snap-length; ignoring newline at start of the first label 
        snaps_offsets[snaps_offsets_idx++] =  (i - last_snap_start) + (i + 1 >= snaps_file.size); 
      }
    }
    i++;
  }
#undef matches_label
}



char* build_fred_output(PieceTable* table, FileBuf* fb, AddBuf* ab, size_t output_len)
{
  char* output_buf = malloc((output_len ? output_len : 1) * sizeof(*output_buf));
  assert_(output_buf != NULL, "not enough memory");

  if (!output_len) {
    output_buf[0] = '\0'; 
    // NOTE: not returning a string literal cause 
    // we would need an extra check when freeing it
  } else {
    size_t offset = 0;
    for (size_t i = 0; i < table->len; i++) {
      Piece* p = &table->items[i];
      char* buf = !p->which_buf ? fb->text : ab->items;
      strncpy(output_buf + offset, buf + p->offset, p->len);
      offset += p->len;
    }
  }
  return output_buf;
}




// DESC: print info and highlight differences between 
// fred's output and given snapshot
void test_failure(FredEditor* fe, size_t key_num, char* key, size_t snap_num,
  char* fred_output, size_t fred_output_len,
  char* snap, size_t snap_len, char* msg)
{
#define SEP "------------------------------------------------\n"
#define SEP2 "------------------------------------------------\n\n"
#define RED "\x1b[38:5:196m"
#define RED_BG "\x1b[48:5:196m"
#define GREEN "\x1b[38:5:48m"
#define YELLOW "\x1b[38:5:220m"
#define LILAC "\x1b[38:5:183m"
#define RESET "\x1b[0m"
#define print(...) do { fprintf(stderr, __VA_ARGS__);} while(0)
#define fred_label() do { \
  print(RED "[fred_output; length: %ld; cursor: %ld,%ld]\n" RESET SEP, \
        fred_output_len, cr->prev_row + 1, cr->prev_col + 1); \
} while (0)
#define snap_label() do { \
  print(GREEN "[snapshot: %ld; length: %ld; cursor: %ld,%ld]\n" RESET SEP, \
        snap_num + 1, snap_len, snap_cr_row, snap_cr_col); \
} while (0)


  Cursor* cr = &fe->cursor;
  size_t snap_cr_row = curs_coords[snap_num * 2];
  size_t snap_cr_col = curs_coords[snap_num * 2 + 1];

  print(RED_BG "TEST FAILED" RESET ": %s\n\n", test_dir_path);
  print(LILAC 
        "NOTE: we are reporting the cursor coordinates\n"
        "      BEFORE the edit. So if 'a' is typed at '1,1',\n"
        "      the test reports 'cursor: 1,1', even tho\n"
        "      now it's at '1,2'.\n\n" RESET);
  print(LILAC 
        "NOTE: on empty snap/output (length 0), a newline\n"
        "      gets still printed between the separators.\n\n" RESET);
  print(LILAC 
        "NOTE: non readable chars like newline and tabs are\n"
        "      are not highlighted.\n\n" RESET);

  print("%s.\n\n", msg);
  print("Inserting key (num: %ld): ", key_num);
  print_key(stderr, key[0], true);
  print("\n");

  if (fred_output_len > snap_len) {
    size_t diff = fred_output_len - snap_len; 
    fred_label();
    print("%.*s", (int)(fred_output_len - diff), fred_output);
    print(RED "%.*s\n" RESET SEP2, (int)(diff), fred_output + (fred_output_len - diff));
    snap_label();
    print("%.*s\n" SEP2, (int)snap_len, snap);
  } else if (fred_output_len < snap_len) {
    fred_label();
    print("%.*s\n" SEP2, (int)fred_output_len, fred_output);
    size_t diff = snap_len - fred_output_len;
    snap_label();
    print("%.*s", (int)(snap_len - diff), snap);
    print(GREEN "%.*s\n" RESET SEP2, (int)(diff), snap + (snap_len - diff));
  } else {
    fred_label();
    for (size_t i = 0; i < fred_output_len; i++) {
      if (fred_output[i] != snap[i]) print(RED);
      print("%c" RESET, fred_output[i]);
    }
    print("\n" SEP2);
    snap_label();
    for (size_t i = 0; i < fred_output_len; i++) {
      if (fred_output[i] != snap[i]) print(GREEN);
      print("%c" RESET, snap[i]);
    }
    print("\n" SEP2);
  }

  if (snap_num > 0) { // NOTE: 'ps' -> previous snapshot
    snap_num--;
    size_t ps_start = snaps_offsets[snap_num * 2];
    size_t ps_len = snaps_offsets[snap_num * 2 + 1];
    char* ps = snaps_file.txt + ps_start;
    size_t ps_cr_row = curs_coords[snap_num * 2];
    size_t ps_cr_col = curs_coords[snap_num * 2 + 1];
    print(YELLOW "[previous snapshot: %ld (successful); length: %ld; cursor: %ld,%ld] \n" RESET SEP, 
          snap_num + 1, ps_len, ps_cr_row, ps_cr_col);
    print("%.*s\n" SEP2, (int)ps_len, ps);
  }

  exit(1);

#undef SEP
#undef SEP2
#undef RED
#undef RED_BG
#undef GREEN
#undef YELLOW
#undef LILAC
#undef RESET
#undef print
#undef fred_label
#undef snap_label
}




// DESC: check if edit happen at the same cursor coordinates, 
// output have the same length and their content is match
void compare_to_snap(FredEditor* fe, size_t key_num, char* key_str, size_t snap_num)
{
  size_t fred_output_len = 0;
  for (size_t i = 0; i < fe->piece_table.len; i++) {
    fred_output_len += fe->piece_table.items[i].len;
  }

  size_t snap_start = snaps_offsets[snap_num * 2];
  size_t snap_len = snaps_offsets[snap_num * 2 + 1];
  char* snap = snaps_file.txt + snap_start;
  size_t snap_row = curs_coords[snap_num * 2];
  size_t snap_col = curs_coords[snap_num * 2 + 1];
  Cursor* cr = &fe->cursor; 

  if (snap_row != (cr->prev_row + 1) || snap_col != (cr->prev_col + 1)) {
    char* fred_output = build_fred_output(&fe->piece_table, &fe->file_buf, &fe->add_buf, fred_output_len);
    char* msg = "Mismatched cursors: edit happened in different places";
    test_failure(fe, key_num, key_str, snap_num, fred_output, fred_output_len, snap, snap_len, msg);
  }

  if (snap_len != fred_output_len) {
    char* fred_output = build_fred_output(&fe->piece_table, &fe->file_buf, &fe->add_buf, fred_output_len);
    char* msg = "Mismatched lengths";
    test_failure(fe, key_num, key_str, snap_num, fred_output, fred_output_len, snap, snap_len, msg);
  }

  if (snap_len == 0) return;

  char* fred_output = build_fred_output(&fe->piece_table, &fe->file_buf, &fe->add_buf, fred_output_len);
  if (0 != strncmp(fred_output, snap, snap_len)) {
    char* msg = "Mismatched characters";
    test_failure(fe, key_num, key_str, snap_num, fred_output, fred_output_len, snap, snap_len, msg);
  }

  free(fred_output);
}




int main(int argc, char* argv[])
{
  if (argc > 2) ERR("momentarily handling one test-folder at a time.");
  else if (argc < 2) ERR("please provide a test-folder path."); 

  test_dir_path = argv[1];

  char* fred_output_path = make_path("fred_output.txt");
  char* output_path = make_path("output.txt");
  char* keys_path = make_path("keys.txt");
  char* snaps_path = make_path("snaps.txt");

  FredEditor fe = {0};
  if (fred_editor_init(&fe, fred_output_path)) exit(1);

  keys = get_keys(keys_path);
  snaps_file = read_file(snaps_path);
  parse_snaps();

  bool _running = true; // NOTE: dummy flag, the tests never generate 'q' for quitting
  bool insert_mode = false;

  for (size_t i = 0, snap_num = 0; i < keys_count; i++) {
    char key_str[2] = {keys[i], '\0'};
    bool just_entered_insert_mode = KEY_IS(key_str, "i") && !insert_mode;

    if (FRED_handle_input(&fe, &_running, &insert_mode, key_str, 1)){
      exit(1);
    }

    if (!just_entered_insert_mode && insert_mode){
      compare_to_snap(&fe, i + 1, key_str, snap_num);
      snap_num++;
    }
  }
  // TODO: time spent, snaps compared 
  printf("\033[48:5:48mTEST PASSED\033[0m\n");

  return 0;
}

