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

  // Occurs exclusively in the v8 thread
  int Execute();

public:
  io::IOReader*       input;

  AVFormatContext*    ictx;
};

}; // transcoding

#endif // NODE_TRANSCODING_QUERYCONTEXT
