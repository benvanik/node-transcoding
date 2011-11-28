#include <node.h>
#include <v8.h>
#include <string>
#include <vector>
#include "utils.h"

#ifndef NODE_TRANSCODING_TASKOPTIONS
#define NODE_TRANSCODING_TASKOPTIONS

using namespace v8;

namespace transcoding {

class StreamingOptions {
public:
  StreamingOptions(Handle<Object> source);
  ~StreamingOptions();

  double            segmentDuration;
  bool              allowCaching;
};

class TaskOptions {
public:
  TaskOptions(Handle<Object> source);
  ~TaskOptions();

  StreamingOptions* streaming;
};

}; // transcoding

#endif // NODE_TRANSCODING_TASKOPTIONS
