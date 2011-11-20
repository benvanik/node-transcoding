#include <node.h>
#include <v8.h>
#include "utils.h"
#include "processor.h"

#ifndef NODE_TRANSCODE_TASK
#define NODE_TRANSCODE_TASK

using namespace v8;

namespace transcode {

class Task : public node::ObjectWrap, public ProcessorSink {
public:
  static void Init(Handle<Object> target);
  static Handle<Value> New(const Arguments& args);

public:
  Task(Handle<Object> source, Handle<Object> target, Handle<Object> options);
  ~Task();

  static Handle<Value> GetSource(Local<String> property,
      const AccessorInfo& info);
  static Handle<Value> GetTarget(Local<String> property,
      const AccessorInfo& info);
  static Handle<Value> GetOptions(Local<String> property,
      const AccessorInfo& info);
  static Handle<Value> GetProgress(Local<String> property,
      const AccessorInfo& info);

  static Handle<Value> Start(const Arguments& args);
  static Handle<Value> Stop(const Arguments& args);

public:
  virtual void EmitBegin(AVFormatContext* ictx, AVFormatContext* octx);
  virtual void EmitProgress(Progress progress);
  virtual void EmitError(int err);
  virtual void EmitEnd();

private:
  Handle<Value> GetProgressInternal(Progress* progress);

private:
  Persistent<Object>      source;
  Persistent<Object>      target;
  Persistent<Object>      options;

  Processor               processor;
};

}; // transcode

#endif // NODE_TRANSCODE_TASK
