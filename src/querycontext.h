#include <node.h>
#include <v8.h>
#include "utils.h"
#include "io/io.h"

#ifndef NODE_TRANSCODING_QUERYCONTEXT
#define NODE_TRANSCODING_QUERYCONTEXT

using namespace v8;

namespace transcoding {

class QueryContext {
public:
  QueryContext(io::IOReader* input);
  ~QueryContext();

  void Abort();

  // Occurs exclusively in the v8 thread
  int Execute();

public:
  pthread_mutex_t     lock;

  bool                running;
  bool                abort;
  int                 err;

  io::IOReader*       input;

  AVFormatContext*    ictx;
};

}; // transcoding

#endif // NODE_TRANSCODING_QUERYCONTEXT
