#include <node.h>
#include <v8.h>
#include <string>
#include <vector>
#include "utils.h"

#ifndef NODE_TRANSCODING_TASKOPTIONS
#define NODE_TRANSCODING_TASKOPTIONS

using namespace v8;

namespace transcoding {

class LiveStreamingOptions {
public:
  LiveStreamingOptions(Handle<Object> source);
  ~LiveStreamingOptions();

  std::string       path;
  std::string       name;

  double            segmentDuration;
  bool              allowCaching;
};

class TaskOptions {
public:
  TaskOptions(Handle<Object> source);
  ~TaskOptions();

  LiveStreamingOptions* liveStreaming;
};

}; // transcoding

#endif // NODE_TRANSCODING_TASKOPTIONS
