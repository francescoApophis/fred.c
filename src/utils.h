#ifndef UTILS_H
#define UTILS_H



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
    tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);         \
    fprintf(stderr, "%s, %d\n", __FILE__, __LINE__);                  \
    fprintf(stderr, "ASSERTION (" #cond ") FAILED:\n");   \
    exit(1);                                              \
  }                                                       \
} while(0)

#define ASSERT_MSG(cond, format, ...) do {                        \
  if (!(cond)){                                           \
    fprintf(stdout, "\033[2J\033[H\n");                   \
    tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);         \
    fprintf(stderr, "ASSERTION (" #cond ") FAILED:\n");   \
    fprintf(stderr, format, __VA_ARGS__);                         \
    fprintf(stderr, "\n");                                \
    exit(1);                                              \
  }                                                       \
} while(0)


#endif 

