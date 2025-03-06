#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "./../src/fred.h"


typedef struct {
  char* text;
  size_t size;
} File;

File read_file(char* filename)
{
  FILE* file = fopen(filename, "rb");
  if (file == NULL){
    fprintf(stderr, "ERROR: could not open file '%s'.\n", filename);
    exit(1);
  }

  fseek(file, 0L, SEEK_END);
  long filesize = ftell(file);
  rewind(file);

  char* buffer = malloc(sizeof(*buffer) * filesize);
  size_t n = fread(buffer, sizeof(char), filesize, file);
  if (n < filesize) {
    fprintf(stderr, "ERROR: could not read file '%s'.\n", filename);
    exit(1);
  }
  fclose(file);

  File f = {.text = buffer, .size = filesize};
  return f;
}


char* get_keys(File* file, size_t* keys_num) 
{
  size_t _keys_num = 0;
  for (size_t i = 0; i < file->size; i++){
    if (file->text[i] == '\n') _keys_num++;
  }
  *keys_num = _keys_num;

  char* keys = malloc(sizeof(*keys) * _keys_num);
  int keys_idx = 0;
  memset(keys, 0, _keys_num * sizeof(*keys));

  char curr_key[3] = {0};
  char* curr_key_digit = curr_key;
  for (size_t i = 0; i < file->size; i++){
    if (file->text[i] == '\n'){
      keys[keys_idx++] = (char)atoi(curr_key);
      memset(curr_key, 0, 3 * sizeof(*curr_key));
      curr_key_digit = curr_key;
      continue;
    }
    *(curr_key_digit++) = file->text[i];
  }
  
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


int main(int argc, char* argv[])
{
  
  if (argc > 2){
    fprintf(stderr, "ERROR: momentarily handling only 1 file.\n");
    exit(1);
  } else if (arg < 2) {
    fprintf(stderr, "ERROR: please provide a file.\n");
    exit(1);
  }

  char* filename = argv[1];
  File file = read_file(filename);
  size_t keys_num = 0;
  char* keys = get_keys(&file, &keys_num);

  print_keys(keys, keys_num);

  free(file.text);
  free(keys);

  return 0;
}

// TODO: return some diagnostics!!!
// What are useful informations in this situation?
// Because right now the way I'm thinking about this test
// is to generate a Fred-output-file which I'm going to 
// compare to the test-output-file. 
// This only tells me that they are different, it does not 
// tell me WHEN they started to diverge (i.e. adding or removing 
// WHICH piece caused the failure).
// It can only tell me AT which piece the difference is only AFTER
// the full table is generated (this i can do by walking the table 
// rather than generating a final char array an comparing it to the 
// test-output-file).
//
// To achieve the WHEN, the only thing that comes to mind is having
// some kind of SCREENSHOT.
// The dumbest way to achieve that would be to feed a key, and after the 
// piece-table got edited, you construct the char array out of the current
// table's state. But then you would need the counterpart screenshot 
// from the test itself to compare the screenshot.
//
// How do i generate the test-screenshots through nvim?
// I would either need two sets of multiple file-screenshots, or two big ass 
// files (one from fred and the other from neovim)in which each screenshot 
// is separated by something.
// Actually i think two big ass files are fine: in neovim the actual text keys
// is randomized differently from the navigation-keys (these are randomly chosen
// once and then repeatedly added for some random amount of times).
// The text keys being different each time makes it unlikely to generate a long
// strip of '-' right? So i can separate the screenshots in a file with 
// '-----------------------' or '[----screenshot_1----]' (literally write 'screenshot' in it, 
// the likelyhood that neovim randomly writes 'screenshot' is absolutely low;
// i can use strncmp to check for it).
//
// THE PROBLEM IS I would need to generate the screenshots after nvim_feedkeys() somehow (shit is
// blocking and a mess to deal with for me). 
// SO either i find a way to deal with nvim_feedkeys() or i ditch it completely, meaning that i would 
// have to rely on vim.fn.cursor() and nvim_buf_set_text().
// If i ditch the thing, I think i have to parse the nvim_feed while keeping track of the 
// mode (check for 'i'/'ESC'). This complicates the test-generation little bit.
//
// So i do the same process with fred, meaning i generate screenshot after 
// editing the table and compare the screenshot with the nvim-screenshot file.
// If there is any difference i report the just-fed-key, the piece, the screenshot_descriptor 
// and maybe even a substring of where the difference is from both screenshots.
//
// Also this is incredibly slow I guess, but maybe it's fine because this 
// is a test??? I'm not checking for speed!!
//
// This all thing means that i need to organize a test in folders.
//

// TODO: mayke a test that deletes all characters in file to check if they are 
// properly deleted
// TODO: make test to check for eventual 0-len pieces
// TODO: right now the test/ folder is a mess. Organize it a little better
// TODO: accept only files named with 'test' and that are '.txt'
// TODO: btw i don't need to use specifically FRED_start_editor(),
// i can just make a separate one that doesn't render nor read from
// stdin. Tho it i need to find a way to  like 'embed'(?) the test. Because 
// right now i cannot just use FRED_start_editor() for the reason that 
// it uses read() and render() funcs. Should i litter the function definition
// with #ifdef for testing? How do people do that? Or do they have separate
// function just like i'm planning to do? Like i'm  okay with making a 
// separate function for the moment, as i'm testing the piece-table and 
// not the FRED_start_editor(), but it would be nice to be able to test it
// right out of the box. 
// TODO: Do i already have a File struct in main.c?
