#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>

#include "./../src/fred.h"


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



char* make_path(const char* dir_path, const char* file_name)
{
  size_t n = strlen(dir_path);
  size_t m = strlen(file_name);
  bool has_sep = dir_path[n - 1] == '/';
  char* full_path = malloc((n + m + (!has_sep ? 2 : 1)) * sizeof(*full_path));

  strncat(full_path, dir_path, n);
  if (!has_sep) strcat(full_path, "/");
  return strncat(full_path, file_name, m);
}

void assert_is_dir(const char* path)
{
  struct stat sb;
  if (stat(path, &sb) == -1) {
    if (errno == ENOENT) ERR("no such test-dir '%s'.", path); // FIXME:  this should say 'ERROR on 'filepath', no such ...'
    ERR("failed to retrieve any info about test-dir '%s', %s.", path, strerror(errno));
  }
  if ((sb.st_mode & S_IFMT) != S_IFDIR){
    ERR("path '%s' is not a directory.", path);
  }
}


void assert_file_exists(const char* path)
{
  struct stat sb;
  if (stat(path, &sb) == -1) {
    // TODO: print all files needed for the fred_tests
    if (errno == ENOENT) ERR("no such test-file '%s'.", path);
    ERR("failed to retrieve any info about test-file '%s', %s.", path, strerror(errno));
  }
  if ((sb.st_mode & S_IFMT) != S_IFREG){
    if ((sb.st_mode & S_IFMT) == S_IFDIR) ERR("given test-file-path '%s' is a directory, not a file.", path);
    ERR("given test-file-path '%s' is not a regular file.", path);
  }
}


typedef struct {
  char* text;
  size_t size;
} File;


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
  size_t n = fread(buffer, sizeof(char), filesize, file);
  if (n < filesize) {
    fprintf(stderr, "ERROR: could not read file '%s'.\n", filename);
    exit(1);
  }
  buffer[n] = '\0';
  fclose(file);

  File f = {.text = buffer, .size = filesize};
  return f;
}


char* get_keys(const char* file_name, size_t* keys_count) 
{
  File file = read_file(file_name);

  for (size_t i = 0; i < file.size; i++){
    if (file.text[i] == '\n') (*keys_count)++;
  }

  // TODO: should make room for a '\0'? I already have keys_count tho
  char* keys = malloc(sizeof(*keys) * (*keys_count));
  memset(keys, 0, (*keys_count) * sizeof(*keys));

  int keys_idx = 0;
  char curr_key[3] = {0};
  char* curr_key_digit = curr_key;
  for (size_t i = 0; i < file.size; i++){
    // TODO: i could use strtol
    if (file.text[i] == '\n'){
      keys[keys_idx++] = (char)atoi(curr_key);
      memset(curr_key, 0, 3 * sizeof(*curr_key));
      curr_key_digit = curr_key;
      continue;
    }
    *(curr_key_digit++) = file.text[i];
  }

  free(file.text);
  
  return keys;
}


void print_key(FILE* stream, char c) {
  switch(c) {
    case 127: { fprintf(stream, "BACKSPACE"); return; }
    case 27:  { fprintf(stream, "ESC"); return; }
    case 10:  { fprintf(stream, "NEWLINE"); return; }
    case 9:   { fprintf(stream, "TAB"); return; }
    default:  { fprintf(stream, "%c", c); return; }
  }
}

void print_keys(char* keys, size_t keys_num)
{
  for (size_t i = 0; i < keys_num; i++){
    print_key(stdout, keys[i]);
  }
}


// NOTE: curs_coords is a 1d array, for each snap two slots 
// contain the row and column of the cursor 
void get_cursor_pos(char* snaps_file, size_t end_sep_idx, size_t* curs_coords, size_t* curs_coords_idx, size_t max_curs_coords)
{
  int semicolon_idx = 0;
  while (snaps_file[end_sep_idx] != ' ') {
    if (snaps_file[end_sep_idx] == ':') semicolon_idx = end_sep_idx;
    end_sep_idx--;
  }
  int row = strtol(snaps_file + end_sep_idx, NULL, 10);
  int col = strtol(snaps_file + semicolon_idx + 1, NULL, 10);

  assert_(*curs_coords_idx < max_curs_coords + 1, 
         "curs_cords_idx: %ld, max_curs_coords: %ld", *curs_coords_idx, max_curs_coords);

  curs_coords[*curs_coords_idx] = row;
  curs_coords[*curs_coords_idx + 1] = col;
  (*curs_coords_idx) += 2;
}


// NOTE: snaps_offsets is a 1d array, for each snap two slots 
// contain the snap-start-offset and the snap-length of the 
size_t get_snaps_offsets(File* snaps_file, size_t** snaps_offsets, size_t** curs_coords)
{
#define matches_sep(ch) ((ch) == '\n' && 0 == strncmp(&(ch), sep, sep_len))

  const char* sep = "\n[snapshot: ";
  const size_t sep_len = strlen(sep);
  size_t snaps_count = 0;
  
  for (int i = snaps_file->size; i >= 0 ; i--){
    if (matches_sep(snaps_file->text[i])){
      char* count_str = &snaps_file->text[(size_t)i + sep_len];
      snaps_count = (size_t)strtol(count_str, NULL, 10);
      break;
    }
  }

  size_t curs_coords_idx = 0;
  *curs_coords = malloc(snaps_count * 2 * sizeof(*curs_coords));

  size_t snaps_offsets_idx = 0;
  *snaps_offsets = malloc(snaps_count * 2 * sizeof(**snaps_offsets));

  size_t i = 0;
  size_t last_snap_start = 0; 
  bool parsing_sep = false;
  while(i < snaps_file->size){
    if (parsing_sep && snaps_file->text[i] == '\n') { // reached end of sep
      
      // get_cursor_pos(snaps_file->text, i, *curs_coords, &curs_coords_idx, snaps_count * 2);
      
      parsing_sep = false;
      last_snap_start = i + 1;
      (*snaps_offsets)[snaps_offsets_idx++] = last_snap_start;
    }

    if (matches_sep(snaps_file->text[i]) || i + 1 >= snaps_file->size){
      parsing_sep = true;
      if (i > 0) { 
        // save snap-length; ignore newline at start of first sep 
        (*snaps_offsets)[snaps_offsets_idx++] =  (i - last_snap_start) + (i + 1 >= snaps_file->size); 
      }
    }
    i++;
  }
  return snaps_count;

#undef matches_sep
}




void test_failure(FredEditor* fe, const char* dir_path, const char* key, size_t snap_num,
                  char* fred_output, size_t fred_output_len,
                  const char* snap, size_t snap_len)
{
#define print(...)   do { fprintf(stderr, __VA_ARGS__);} while(0)
#define sep_end(...) do { print(__VA_ARGS__ "--------------------------------------------\n\n\n"); } while(0)
#define sep_start()  do { print("--------------------------------------------\n"); } while(0)
#define set_red()    do { print("\033[1m\033[38:5:196m"); } while (0)
#define set_green()  do { print("\033[1m\033[38:5:48m"); } while (0)
#define reset()      do { print("\033[0m"); } while (0)
#define fred_label() do { \
  set_red(); print("[fred-output, length: %ld]\n", fred_output_len); reset(); sep_start(); } while(0)
#define snap_label() do { \
  set_green(); print("[snapshot: %ld, length: %ld]\n", snap_num, snap_len); reset(); sep_start(); } while(0)

  print("\033[48:5:160mTEST FAILED\033[0m: %s\n", dir_path);
  print("inserting char: '");
  print_key(stderr, key[0]);
  print("' (%d);\n", key[0]);

  // FIXME: on non-visible char there is a chance that other 
  // or no chars at all will get highlighted
  
  if (fred_output_len > snap_len) {
    fred_label();
    for (size_t i = 0; i < fred_output_len; i++) {
      if (i >= snap_len) set_red();
      print("%c", fred_output[i]);
    }
    reset();
    sep_end("\n");
    snap_label();
    if (snap_len) print("%.*s\n", (int)snap_len, snap);
    sep_end();

  } else if (fred_output_len < snap_len) {
    fred_label();
    if (fred_output_len) print("%.*s\n", (int)fred_output_len, fred_output);
    sep_end();
    snap_label();
    for (size_t i = 0; i < snap_len; i++) {
      if (i >= fred_output_len) set_green();
      print("%c", snap[i]);
    }
    reset();
    sep_end("\n");

  } else {
    fred_label();
    for (size_t i = 0; i < fred_output_len; i++) {
      if (fred_output[i] == snap[i]) {
        print("%c", fred_output[i]);
      } else {
        set_red();
        print("%c", fred_output[i]);
        reset();
      }
    }
    sep_end("\n");
    snap_label();
    for (size_t i = 0; i < snap_len; i++) {
      if (fred_output[i] == snap[i]) {
        print("%c", snap[i]);
      } else {
        set_green();
        print("%c", snap[i]);
        reset();
      }
    }
    sep_end("\n");
  }

  exit(1);

#undef print
#undef sep
#undef sep2
#undef set_red
#undef set_green
#undef reset
#undef fred_label
#undef snap_label
}



void build_fred_output(FredEditor* fe, char* dest)
{
  size_t offset = 0;
  for (size_t i = 0; i < fe->piece_table.len; i++) {
    Piece* p = &fe->piece_table.items[i];
    char* buf = !p->which_buf ? fe->file_buf.text : fe->add_buf.items;
    strncpy(dest + offset, buf + p->offset, p->len);
    offset += p->len;
  }
}

void compare_to_snap(FredEditor* fe, const char* dir_path, char* key_str, char* snaps, size_t* snaps_offsets, size_t snap_num)
{
  size_t fred_output_len = 0;
  for (size_t j = 0; j < fe->piece_table.len; j++) {
    fred_output_len += fe->piece_table.items[j].len;
  }

  size_t snap_start = snaps_offsets[snap_num * 2];
  size_t snap_len = snaps_offsets[snap_num * 2 + 1];
  char* snap = snaps + snap_start;

  if (snap_len != fred_output_len) {
    if (fred_output_len > 0) {
      char fred_output[fred_output_len];
      build_fred_output(fe, fred_output);
      test_failure(fe, dir_path, key_str, snap_num + 1, fred_output, fred_output_len, snap, snap_len);
    }
    char fred_output[1] = {'\0'};
    test_failure(fe, dir_path, key_str, snap_num + 1, fred_output, fred_output_len, snap, snap_len);
  }

  if (snap_len == 0) return;

  char fred_output[fred_output_len];
  build_fred_output(fe, fred_output);

  if (0 != strncmp(fred_output, snap, snap_len)) {
    test_failure(fe, dir_path, key_str, snap_num + 1, fred_output, fred_output_len, snap, snap_len);
  }
}

int main(int argc, char* argv[])
{
  // TODO: explaing that the test only accept 'fred_test_[n]'-like folders
  // and that it only tests the piece-table
  if (argc > 2) ERR("momentarily handling one test-folder at a time.");
  else if (argc < 2) ERR("please provide a test-folder path."); 
  
  const char* dir_path = argv[1];

  // TODO: all fred_test_[n] folders live inside tests/. Attach './' to path?
  assert_is_dir(dir_path);

  // TODO: fred_editor_init needs exisising-path-file, piece-table shouldn't
  char* fred_output_path = make_path(dir_path, "fred_output.txt");
  char* output_path = make_path(dir_path, "output.txt");
  char* keys_path = make_path(dir_path, "keys.txt");
  char* snaps_path = make_path(dir_path, "snaps.txt");

  assert_file_exists(fred_output_path);
  assert_file_exists(output_path);
  assert_file_exists(keys_path);
  assert_file_exists(snaps_path);

  // TODO: fred_editor_init needs exisising-path-file, piece-table shouldn't
  FredEditor fe = {0};
  if (fred_editor_init(&fe, fred_output_path)) exit(1);

  size_t keys_count = 0;
  char* keys = get_keys(keys_path, &keys_count);

  File snaps = read_file(snaps_path);
  size_t* curs_coords = NULL;
  size_t* snaps_offsets = NULL;
  size_t snaps_count = get_snaps_offsets(&snaps, &snaps_offsets, &curs_coords);

  bool _running = true; // NOTE: dummy flag, the tests never generate 'q' for quitting
  bool insert_mode = false;
  for (size_t i = 0, snap_num = 0; i < keys_count; i++) {
    char key_str[2] = {keys[i], '\0'};
    bool just_entered_insert_mode = KEY_IS(key_str, "i") && !insert_mode;

    if (FRED_handle_input(&fe, &_running, &insert_mode, key_str, 1)){
      exit(1);
    }

    if (!just_entered_insert_mode && insert_mode){
      compare_to_snap(&fe, dir_path, key_str, snaps.text, snaps_offsets, snap_num);
      snap_num++;
    }
  }
  printf("\033[48:5:48mTEST PASSED\033[0m\n");

  // dump_piece_table(&fe, stderr);
  // TODO: we exit right away on text-failure and don't free anything.
  // Doing it here makes no sense
  // dump_piece_table(&fe, stdout);
  fred_editor_free(&fe);
  free(keys);
  free(fred_output_path);
  free(output_path);
  free(keys_path);
  free(snaps_path);
  free(curs_coords);
  free(snaps_offsets);

  return 0;
}


// TODO: print the position of the fred-cursor and test-cursor

// TODO: better naming for everything


// TODOOOOOOOOOOOOOOOOOOOOOOOOOOOO: check if every malloc has been freed

// TODO: accept a '-h' flag that explains everything, from what's needed to what is happening
// TODO: when in the future multiple tests will be run at the same time 
// remember to free each File struct.

// TODO: make test to check for eventual 0-len pieces
// TODO: accept only files named with 'test' and that are '.txt'
