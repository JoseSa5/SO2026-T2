#ifndef DEBUG_H
#define DEBUG_H

 




#ifndef NDEBUG
  #define DBG(fmt, ...) \
  fprintf(stderr, "[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
  #define DBG(...) ((void)0)
#endif

#endif  
