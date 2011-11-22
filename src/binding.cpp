#include <node.h>
#include <v8.h>
#include "utils.h"
#include "io.h"
#include "mediainfo.h"
//#include "task.h"

using namespace transcode;
using namespace v8;

namespace transcode {

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

static Handle<Value> queryInfo(const Arguments& args) {
  HandleScope scope;

  Local<Object> source = args[0]->ToObject();
  IOHandle* input = IOHandle::Create(source);

  Local<Function> callback = args[1].As<Function>();

  int ret = 0;
  AVFormatContext* ctx = createInputContext(input, &ret);
  if (ret) {
    // Failed to open/parse
    char buffer[256];
    av_strerror(ret, buffer, sizeof(buffer));
    Handle<Value> argv[] = {
      Exception::Error(String::New(buffer)),
      Undefined(),
    };
    callback->Call(Context::GetCurrent()->Global(), countof(argv), argv);
  } else {
    // Generate media info
    //av_dump_format(ctx, 0, NULL, 0);
    Local<Object> result = Local<Object>::New(createMediaInfo(ctx, false));

    Handle<Value> argv[] = {
      Undefined(),
      result,
    };
    callback->Call(Context::GetCurrent()->Global(), countof(argv), argv);
  }

  delete input;

  return scope.Close(Undefined());
}

}; // transcode

extern "C" void node_transcode_init(Handle<Object> target) {
  HandleScope scope;

  // One-time prep
  av_register_all();
  av_log_set_level(AV_LOG_QUIET);

  //transcode::Task::Init(target);

  NODE_SET_METHOD(target, "setDebugLevel", transcode::setDebugLevel);
  NODE_SET_METHOD(target, "queryInfo", transcode::queryInfo);
}

NODE_MODULE(node_transcode, node_transcode_init);
