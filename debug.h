#ifndef DEBUG
#define DEBUG

extern int debug;

#define DLOG(fmt) \
  do { if(debug) fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, __LINE__, \
          __FUNCTION__); } while(0)

#endif
