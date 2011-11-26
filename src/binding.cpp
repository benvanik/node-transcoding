#include <node.h>
#include <v8.h>
#include "utils.h"
#include "query.h"
#include "task.h"
#include "io/io.h"

using namespace transcoding;
using namespace transcoding::io;

namespace transcoding {

static Handle<Value> setDebugLevel(const Arguments& args) {
  HandleScope scope;

  Local<Boolean> debugLog = args[0]->ToBoolean();
  if (debugLog->Value()) {
    av_log_set_level(AV_LOG_DEBUG);
  } else {
    av_log_set_level(AV_LOG_QUIET);
  }

  return scope.Close(Undefined());
}

}; // transcoding

extern "C" void node_transcoding_init(Handle<Object> target) {
  HandleScope scope;

  // One-time prep
  av_register_all();
  av_log_set_level(AV_LOG_QUIET);

  transcoding::Query::Init(target);
  transcoding::Task::Init(target);

  NODE_SET_METHOD(target, "setDebugLevel", transcoding::setDebugLevel);
}

NODE_MODULE(node_transcoding, node_transcoding_init);
