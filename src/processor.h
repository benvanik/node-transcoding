#include <pthread.h>
#include <node.h>
#include <v8.h>
#include "utils.h"
#include "io.h"

#ifndef NODE_TRANSCODE_PROCESSOR
#define NODE_TRANSCODE_PROCESSOR

using namespace v8;

namespace transcode {

class ProcessorOptions {
public:
  ProcessorOptions(Handle<Object> source);
  ~ProcessorOptions();
};

typedef struct Progress_t {
  double    timestamp;
  double    duration;
  double    timeElapsed;
  double    timeEstimated;
  double    timeRemaining;
  double    timeMultiplier;
} Progress;

class ProcessorSink {
protected:
  ProcessorSink() {}
public:
  virtual ~ProcessorSink() {}

  virtual void EmitBegin(AVFormatContext* ictx, AVFormatContext* octx) = 0;
  virtual void EmitProgress(Progress progress) = 0;
  virtual void EmitError(int err) = 0;
  virtual void EmitEnd() = 0;
};

class Processor {
public:
  Processor();
  ~Processor();

  void SetSink(ProcessorSink* sink);

  void Execute(InputDescriptor* input, OutputDescriptor* output,
      ProcessorOptions* options, Handle<Object> obj);

  Progress GetProgress();
  void Abort();

private:
  void Ref();
  void Unref();

  static AVStream* AddOutputStreamCopy(AVFormatContext* octx, AVStream* istream,
      int* pret);

  static void ThreadWorker(uv_work_t* request);
  static void ThreadWorkerComplete(uv_work_t* request);

  static void EmitBegin(uv_async_t* handle, int status);
  static void EmitBeginClose(uv_handle_t* handle);
  static void EmitProgress(uv_async_t* handle, int status);
  static void EmitProgressClose(uv_handle_t* handle);
  static void EmitError(uv_async_t* handle, int status);
  static void EmitErrorClose(uv_handle_t* handle);
  static void EmitEnd(uv_async_t* handle, int status);
  static void EmitEndClose(uv_handle_t* handle);

private:
  Persistent<Object>  obj;
  ProcessorSink*      sink;
  pthread_mutex_t     lock;

  Progress            progress;

  // All guarded by lock
  bool                running;
  bool                abort;
  int                 refs;
  InputDescriptor*    input;
  OutputDescriptor*   output;
  ProcessorOptions*   options;
  AVFormatContext*    ictx;
  AVFormatContext*    octx;
};

}; // transcode

#endif // NODE_TRANSCODE_PROCESSOR
