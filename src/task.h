#include <node.h>
#include <v8.h>
#include "utils.h"
#include "profile.h"

#ifndef NODE_TRANSCODE_TASK
#define NODE_TRANSCODE_TASK

using namespace v8;

namespace transcode {

typedef struct Progress_t {
  double    timestamp;
  double    duration;
  double    timeElapsed;
  double    timeEstimated;
  double    timeRemaining;
  double    timeMultiplier;
} Progress;

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
  virtual void EmitBegin(AVFormatContext* ictx, AVFormatContext* octx);
  virtual void EmitProgress(Progress progress);
  virtual void EmitError(int err);
  virtual void EmitEnd();

private:
  Handle<Value> GetProgressInternal(Progress* progress);

private:
  Persistent<Object>      source;
  Persistent<Object>      target;
  Persistent<Object>      profile;
  Persistent<Object>      options;
};

}; // transcode

#endif // NODE_TRANSCODE_TASK
