#include <node.h>
#include <v8.h>
#include "utils.h"
#include "io.h"
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

class TaskContext {
public:
  TaskContext(IOHandle* input, IOHandle* output, Profile* profile);
  ~TaskContext();

  Progress GetProgress();
  void Abort();

public:
  pthread_mutex_t     lock;

  bool                running;
  bool                abort;

  IOHandle*           input;
  IOHandle*           output;
  Profile*            profile;
  // options

  Progress            progress;

  AVFormatContext*    ictx;
  AVFormatContext*    octx;
};

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
  void Complete();

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
