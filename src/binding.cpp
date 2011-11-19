#include <node.h>
#include <v8.h>
#include "utils.h"
#include "mediainfo.h"
#include "task.h"

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

static AVFormatContext* openMedia(const char* sourcePath, int* pret) {
  AVFormatContext* ctx = NULL;
  int ret = 0;
  *pret = 0;

  ret = avformat_open_input(&ctx, sourcePath, NULL, NULL);
  if (ret < 0) {
    goto CLEANUP;
  }

  ret = av_find_stream_info(ctx);
  if (ret < 0) {
    goto CLEANUP;
  }

  return ctx;

CLEANUP:
  if (ctx) {
    avformat_free_context(ctx);
  }
  *pret = ret;
  return NULL;
}

static Handle<Value> queryInfo(const Arguments& args) {
  HandleScope scope;

  Local<String> source = args[0]->ToString();
  String::AsciiValue asciiSourcePath(source);

  Local<Function> callback = args[1].As<Function>();

  int ret = 0;
  AVFormatContext* ctx = openMedia(*asciiSourcePath, &ret);
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

    avformat_free_context(ctx);

    Handle<Value> argv[] = {
      Undefined(),
      result,
    };
    callback->Call(Context::GetCurrent()->Global(), countof(argv), argv);
  }

  return scope.Close(Undefined());
}

}; // transcode

extern "C" void node_transcode_init(Handle<Object> target) {
  HandleScope scope;

  // One-time prep
  av_register_all();
  av_log_set_level(AV_LOG_QUIET);

  transcode::Task::Init(target);

  NODE_SET_METHOD(target, "setDebugLevel", transcode::setDebugLevel);
  NODE_SET_METHOD(target, "queryInfo", transcode::queryInfo);
}

NODE_MODULE(node_transcode, node_transcode_init);
