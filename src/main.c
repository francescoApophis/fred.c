#include "common.h"
#include "fred.h"


termios term_orig = {0};
struct sigaction act = {0};
struct sigaction old = {0};


void handle_win_resize_sig(int sig)
{
  (void)sig;
}

bool setup_terminal()
{
  bool failed = 0;
  bool term_set = 0;

  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)){
    ERROR("Fred editor can only be run in a terminal.");
  }
  
  if (tcgetattr(STDIN_FILENO, &term_orig) == -1){
    ERROR("failed to retrieve terminal options. %s.", strerror(errno));
  }

  termios term_raw = term_orig;
  term_raw.c_iflag &= ~(BRKINT | ISTRIP | INPCK |  IXON); // ICRNL |
  term_raw.c_lflag &= ~(ICANON | IEXTEN | ECHO);
  term_raw.c_cflag &= ~(CSIZE | PARENB);
  term_raw.c_cflag |= CS8;
  term_raw.c_cc[VTIME] = 5;

  if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_raw)){
    ERROR("failed to set terminal options. %s.", strerror(errno));
  }

  act.sa_handler = handle_win_resize_sig;
  if (-1 == sigaction(SIGWINCH, &act, &old)){
    ERROR("failed to set up the editor to detect window changes. %s.", strerror(errno));
  }

  GOTO_END(failed);
end:
  if (failed && term_set) {
    tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
  }
  return failed;
}



int main(int argc, char** argv)
{
  // REMEMBER: JUST MAKE SOMETHING THAT WORKS FIRST!!!!!!!!
  bool failed = 0;

  if (argc < 2) ERROR("no file-path provided.");
  if (argc > 2) ERROR("too many arguments; can only handle one file right now.");

  char* file_path = argv[1];

  bool term_and_sig_set = 0;
  failed = setup_terminal();
  if (failed) GOTO_END(1);
  term_and_sig_set = 1;

  FredEditor fe = {0};
  failed = fred_editor_init(&fe, file_path);
  if (failed) GOTO_END(1);
  
  failed = FRED_start_editor(&fe, file_path);
  if (failed) GOTO_END(1);

  GOTO_END(failed);
end:
  if (term_and_sig_set) {
    tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
    sigaction(SIGWINCH, &old, NULL);
  }
  return failed;
}

// FIXME: '\t' is not rendered properly
// FIXME: SIGWINCH doesn't handle resizing on zooming in/out when
// the terminal is not full screen?
// FIXME: fred sometimes crashes with empty file even though that should already be handled
// TODO: utf-8 support.
// TODO: handle ftruncate error
// TODO: have a separate buffer to report error messages in the 
// bottom line of the screen
// TODO: handle eintr on close() in open_file()
// TODO: handle failures in render_text()
// TODO: in the future if i decide to add custom key-maps, I could 
// use an array with designated initializers to hold mappings
// mappings = { [ESC_KEY] = void (*some_job)(void*)...,
             // [SPACE_KEY] = void (*some_job)(void*)...,}
// where ESC_KEY is a preproc constant



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
