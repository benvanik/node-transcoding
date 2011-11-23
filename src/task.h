#include <node.h>
#include <v8.h>
#include "utils.h"
#include "io.h"
#include "profile.h"
#include "taskcontext.h"

#ifndef NODE_TRANSCODE_TASK
#define NODE_TRANSCODE_TASK

using namespace v8;

namespace transcode {

class Task : public node::ObjectWrap {
public:
  static void Init(Handle<Object> target);
  static Handle<Value> New(const Arguments& args);

public:
  Task(Handle<Object> source, Handle<Object> target, Handle<Object> profile,
      Handle<Object> options);
  ~Task();

  static Handle<Value> GetSource(Local<String> property,
      const AccessorInfo& info);
  static Handle<Value> GetTarget(Local<String> property,
      const AccessorInfo& info);
  static Handle<Value> GetProfile(Local<String> property,
      const AccessorInfo& info);
  static Handle<Value> GetOptions(Local<String> property,
      const AccessorInfo& info);
  static Handle<Value> GetProgress(Local<String> property,
      const AccessorInfo& info);

  static Handle<Value> Start(const Arguments& args);
  static Handle<Value> Stop(const Arguments& args);

public:
  void EmitBegin(AVFormatContext* ictx, AVFormatContext* octx);
  void EmitProgress(Progress progress);
  void EmitError(int err);
  void EmitEnd();

  static void EmitProgressAsync(uv_async_t* handle, int status);
  static void EmitCompleteAsync(uv_async_t* handle, int status);
  static void AsyncHandleClose(uv_handle_t* handle);

  static void ThreadWorker(uv_work_t* request);

private:
  Handle<Value> GetProgressInternal(Progress* progress);

private:
  Persistent<Object>  source;
  Persistent<Object>  target;
  Persistent<Object>  profile;
  Persistent<Object>  options;

  TaskContext*        context;
};

}; // transcode

#endif // NODE_TRANSCODE_TASK
