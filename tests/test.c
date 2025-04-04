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

#define assert(cond, ...) do { \
  if (!(cond)){ \
    fprintf(stderr, "[%s, line: %d] ASSERTION FAILED '" #cond "':\n", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
    exit(1); \
  } \
} while (0)



const char* get_file_path(const char* folder_path, const char* file_name)
{
  size_t n = strlen(folder_path);
  size_t m = strlen(file_name);
  bool has_sep = folder_path[n - 1] == '/';
  char* full_path = malloc((n + m + (!has_sep ? 2 : 1)) * sizeof(*full_path));

  strncat(full_path, folder_path, n);
  if (!has_sep) strcat(full_path, "/");
  return strncat(full_path, file_name, m);
}

void check_test_folder(const char* folder_path)
{
  struct stat sb;
  if (stat(folder_path, &sb) == -1) {
    if (errno == ENOENT) ERR("no such test-folder '%s'.", folder_path); // FIXME:  this should say 'ERROR on 'filepath', no such ...'
    ERR("failed to retrieve any info about test-folder '%s', %s.", folder_path, strerror(errno));
  }
  if ((sb.st_mode & S_IFMT) != S_IFDIR){
    ERR("path '%s' is not a directory.", folder_path);
  }
}


// TODO: since it stops the program this should be called assert?
// But then it would get confused with an actual assert?
void check_test_file_exists(const char* file_path)
{
  struct stat sb;
  if (stat(file_path, &sb) == -1) {
    // TODO: print all files needed for the fred_tests
    if (errno == ENOENT) ERR("no such test-file '%s'.", file_path);
    ERR("failed to retrieve any info about test-file '%s', %s.", file_path, strerror(errno));
  }
  if ((sb.st_mode & S_IFMT) != S_IFREG){
    if ((sb.st_mode & S_IFMT) == S_IFDIR) ERR("given test-file-path '%s' is a directory, not a file.", file_path);
    ERR("given test-file-path '%s' is not a regular file.", file_path);
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


char* get_keys_from_file(const char* file_name, size_t* keys_count) 
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

void print_keys(char* keys, size_t keys_num)
{
  for (size_t i = 0; i < keys_num; i++){
    if (keys[i] == 127) printf("BACKSPACE\n");
    else if (keys[i] == 10) printf("NEWLINE\n");
    else if (keys[i] == 9) printf("TAB\n");
    else if (keys[i] == 27) printf("ESC\n");
    else printf("%c\n", keys[i]);
  }
}


// TODO+FIX: i could return snaps as File structures
// if ending the array of snaps with a NULL is bad
char** get_snaps_from_file(const char* file_name)
{
#define matches_sep(ch) ((ch) == '\n' && 0 == strncmp(&(ch), sep, sep_len))

  const char* sep = "\n[snapshot: ";
  const size_t sep_len = strlen(sep);
  long int snaps_count = 0;
  File file = read_file(file_name);
  
  for (size_t i = file.size; i >= 0 ; i--){
    if (matches_sep(file.text[i])){
      char* count_str = &file.text[i + sep_len];
      snaps_count = strtol(count_str, NULL, 10);
      break;
    }
  }

  char** snaps = malloc((snaps_count + 1) * sizeof(*snaps));
  size_t snaps_idx = 0;

  size_t i = 0;
  int last_snap_start = 0;
  while(i < file.size){
    if (matches_sep(file.text[i])){
      i++;

      while(i < file.size && file.text[i] != '\n') i++;
      i++;
      last_snap_start = i;

      while(i < file.size && (!matches_sep(file.text[i]))) i++;
      int new_snap_start = i;

      size_t len = new_snap_start - last_snap_start;
      snaps[snaps_idx] = malloc((len + 1) * sizeof(*snaps[snaps_idx]));

      strncpy(snaps[snaps_idx], file.text + last_snap_start, len);
      snaps[snaps_idx][len] = '\0';
      
      last_snap_start = new_snap_start;
      snaps_idx++;
      continue;
    }
    i++;
  }
  snaps[snaps_count] = NULL;
  return snaps;

#undef matches_sep
}


bool feed_key(FredEditor* fe, TermWin* tw, char* key, bool* insert)
{
  bool failed = 0;

  if (*insert){
    if (KEY_IS(key, "\x1b") || KEY_IS(key, "\x1b ")){
      fe->last_edit.cursor = fe->cursor;
      *insert = false;
    } else if (KEY_IS(key, "\x7f")){
      failed = FRED_delete_text(fe);
      if (failed) GOTO_END(1);

    } else{
      failed = FRED_insert_text(fe, key[0]);
      if (failed) GOTO_END(1);
    }

  } else {
    if (KEY_IS(key, "h") || 
      KEY_IS(key, "j") || 
      KEY_IS(key, "k") || 
      KEY_IS(key, "l")) FRED_move_cursor(fe, tw, key[0]);

    // else if (KEY_IS(key, "q")) *running = false; // NOTE: the tests don't need to test for quitting
    else if (KEY_IS(key, "i")) *insert = true;
    else if (KEY_IS(key, "\x1b") || KEY_IS(key, "\x1b ")) *insert = false;
    else printf("no match\n");
  }

  GOTO_END(failed);

end:
  return failed;
}


void test_failure(const char* folder_path, const char* key, int snap_num, const char* snap,  const char* fred_output)
{
  fprintf(stderr, "TEST FAILED:\n");
  fprintf(stderr, "name test: %s\n", folder_path);
  fprintf(stderr, "inserting char: '%c' (%d)\n", key[0], key[0]);
  fprintf(stderr, "\n");
  fprintf(stderr, "snapshot %d:\n", snap_num);
  fprintf(stderr, "----------------\n");
  fprintf(stderr, "%s", snap);
  fprintf(stderr, "\n----------------\n\n");
  fprintf(stderr, "fred output:\n");
  fprintf(stderr, "----------------\n");
  fprintf(stderr, "%s", fred_output);
  fprintf(stderr, "\n----------------\n");
  exit(1);
}


int main(int argc, char* argv[])
{
  // TODO: explaing that the test only accept 'fred_test_[n]'-like folders
  if (argc > 2) ERR("momentarily handling one test-folder at a time.");
  else if (argc < 2) ERR("please provide a test-folder path."); 
  
  const char* folder_path = argv[1];

  // TODO: since all fred_test_[n] folders will live inside tests/, 
  // maybe I should attach './' to folder_path? just use get_file_path("./", folder_path)
  check_test_folder(folder_path);


  // TODO: right now to start up fred_editor_init needs to receive 
  // a path to an existent file. The piece-table shouldn't need that 
  // to be initialized.
  const char* tf_fred_output_name = get_file_path(folder_path, "fred_output.txt");
  const char* tf_output_name = get_file_path(folder_path, "output.txt"); // [t]est [f]ile
  const char* tf_keys_name = get_file_path(folder_path, "keys.txt");
  const char* tf_snaps_name = get_file_path(folder_path, "snaps.txt");

  check_test_file_exists(tf_output_name);
  check_test_file_exists(tf_fred_output_name);
  check_test_file_exists(tf_keys_name);
  check_test_file_exists(tf_snaps_name);


  // TODO: right now to start up fred_editor_init needs to receive 
  // a path to an existent file. The piece-table shouldn't need that 
  // to be initialized.
  FredEditor fe = {0};
  if (fred_editor_init(&fe, tf_fred_output_name))  exit(1);

  // NOTE+TODO: this is needed because at the moment FRED_move_cursor()
  // is also handling the win_cursor movement and this all 
  // thing is too coupled, but in theory it's not needed 
  TermWin tw = {0};
  if (FRED_win_resize(&tw)) exit(1);

  size_t keys_count = 0;
  char* keys = get_keys_from_file(tf_keys_name, &keys_count);
  char* curr_key = keys;
  char** snaps = get_snaps_from_file(tf_snaps_name);
  char** curr_snap = snaps;
  bool insert = false;

  // TODO: rewrite this shit wtf 
  for (size_t i = 0; i < keys_count; i++){
    char key[2] = {*curr_key, '\0'}; // NOTE: feed_key() is emulating FRED_start_editor which handles strings 
    bool maybe_compare_snap = !(KEY_IS(key, "i") && !insert);

    if (1 == feed_key(&fe, &tw, key, &insert)) exit(1);

    if (maybe_compare_snap && insert){

      size_t fred_output_size = 0;
      for (size_t i = 0; i < fe.piece_table.len; i++){
        fred_output_size += fe.piece_table.items[i].len;
      }
      char fred_output[fred_output_size + 1];
      fred_output[0] = '\0';

      for (size_t i = 0; i < fe.piece_table.len; i++){
        Piece* piece = &fe.piece_table.items[i];
        char* buf = (!piece->which_buf? fe.file_buf.text : fe.add_buf.items);
        strncat(fred_output, buf + piece->offset, piece->len);
      }

      if (0 != strcmp(fred_output, *curr_snap)){
        int snap_num = curr_snap - snaps + 1;
        test_failure(folder_path, key, snap_num, *curr_snap, fred_output);
      }
      curr_snap++;
    }
    curr_key++;
  }

  printf("TEST PASSED\n");

  fred_editor_free(&fe);
  free(keys);

  return 0;
}



// TODO: I could print a screenshots-like file to show the cursor movements
// throught the editing 


// TODOOOOOOOOOOOOOOOOOOOOOOOOOOOO: check if every malloc has been freed

// TODO: accept a '-h' flag that explains everything, from what's needed to what is happening
// TODO: when in the future multiple tests will be run at the same time 
// remember to free each File struct.

// TODO: better diagnostics?
//
// TODO: make test to check for eventual 0-len pieces
// TODO: accept only files named with 'test' and that are '.txt'
// TODO: File struct is useless
