#include "processor.h"

using namespace transcode;

typedef struct ProcessorReq_t {
  uv_work_t           request;
  Processor*          processor;
  Persistent<Object>  obj;
} ProcessorReq;

typedef struct ProcessorAsyncBegin_t {
  Processor*          processor;
  AVFormatContext*    ictx;
  AVFormatContext*    octx;
} ProcessorAsyncBegin;

typedef struct ProcessorAsyncProgress_t {
  Processor*          processor;
  Progress            progress;
} ProcessorAsyncProgress;

typedef struct ProcessorAsyncError_t {
  Processor*          processor;
  int                 err;
} ProcessorAsyncError;

typedef struct ProcessorAsyncEnd_t {
  Processor*          processor;
} ProcessorAsyncEnd;

ProcessorOptions::ProcessorOptions(Handle<Object> source) {
  HandleScope scope;
}

ProcessorOptions::~ProcessorOptions() {
}

Processor::Processor() :
    running(false), abort(false), sink(NULL), ictx(NULL), octx(NULL) {
  pthread_mutex_init(&this->lock, NULL);

  memset(&this->progress, 0, sizeof(this->progress));
}

Processor::~Processor() {
  cleanupContext(this->ictx);
  this->ictx = NULL;
  cleanupContext(this->octx);
  this->octx = NULL;
  if (this->input) {
    delete this->input;
  }
  if (this->output) {
    delete this->output;
  }
  if (this->options) {
    delete this->options;
  }
  pthread_mutex_destroy(&this->lock);
  this->sink = NULL;
}

void Processor::SetSink(ProcessorSink* sink) {
  assert(!this->sink);
  assert(sink);
  this->sink = sink;
}

void Processor::Execute(InputDescriptor* input, OutputDescriptor* output,
    ProcessorOptions* options, Handle<Object> obj) {
  HandleScope scope;

  this->input = input;
  this->output = output;
  this->options = options;

  this->running = true;

  ProcessorReq* req = new ProcessorReq();
  req->request.data = req;
  req->processor = this;
  req->obj = Persistent<Object>::New(obj);

  int status = uv_queue_work(uv_default_loop(),
      &req->request, ThreadWorker, ThreadWorkerComplete);
  assert(status == 0);
}

void Processor::ThreadWorker(uv_work_t* request) {
  ProcessorReq* req = static_cast<ProcessorReq*>(request->data);
  Processor* processor = req->processor;

  int ret = 0;

  // Grab contexts
  AVFormatContext* ictx = NULL;
  AVFormatContext* octx = NULL;
  if (!ret) {
    ictx = createInputContext(processor->input, &ret);
    if (ret) {
      cleanupContext(ictx);
      cleanupContext(octx);
    }
  }
  if (!ret) {
    octx = createOutputContext(processor->output, &ret);
    if (ret) {
      cleanupContext(ictx);
      cleanupContext(octx);
    }
  }

  // Setup output container
  if (!ret) {
  }

  // Setup streams
  if (!ret) {
  }

  // Stash away on the processor (they will be cleaned up on destruction)
  // Emit begin
  if (!ret) {
    pthread_mutex_lock(&processor->lock);
    processor->ictx = ictx;
    processor->octx = octx;
    pthread_mutex_unlock(&processor->lock);

    ProcessorAsyncBegin* asyncBegin = new ProcessorAsyncBegin();
    asyncBegin->processor = processor;
    asyncBegin->ictx = ictx;
    asyncBegin->octx = octx;
    NODE_ASYNC_SEND(asyncBegin, EmitBegin);
  }

  // Main pump
  if (!ret) {
    Progress progress;
    memset(&progress, 0, sizeof(progress));
    while (true) {
      // Copy progress (speeds up queries later on)
      // Also grab the current abort flag
      pthread_mutex_lock(&processor->lock);
      bool aborting = processor->abort;
      memcpy(&processor->progress, &progress, sizeof(progress));
      pthread_mutex_unlock(&processor->lock);

      // Emit progress event
      ProcessorAsyncProgress* asyncProgress = new ProcessorAsyncProgress();
      asyncProgress->processor = processor;
      memcpy(&asyncProgress->progress, &progress, sizeof(progress));
      NODE_ASYNC_SEND(asyncProgress, EmitProgress);

      // TODO: work
      break;

      if (!ret || aborting) {
        // Error occurred or abort requested - early exit
        break;
      }
    };
  }

  // Emit error
  if (ret) {
    ProcessorAsyncError* asyncError = new ProcessorAsyncError();
    asyncError->processor = processor;
    asyncError->err = ret;
    NODE_ASYNC_SEND(asyncError, EmitError);
  }

  // Emit end
  ProcessorAsyncEnd* asyncEnd = new ProcessorAsyncEnd();
  asyncEnd->processor = processor;
  NODE_ASYNC_SEND(asyncEnd, EmitEnd);
}

void Processor::ThreadWorkerComplete(uv_work_t* request) {
  ProcessorReq* req = static_cast<ProcessorReq*>(request->data);
  Processor* processor = req->processor;

  processor->running = false;

  req->obj.Dispose();
  delete req;
}

void Processor::EmitBegin(uv_async_t* handle, int status) {
  assert(status == 0);
  ProcessorAsyncBegin* req =
      static_cast<ProcessorAsyncBegin*>(handle->data);

  req->processor->sink->EmitBegin(req->ictx, req->octx);

  NODE_ASYNC_CLOSE(handle, EmitBeginClose);
}

void Processor::EmitBeginClose(uv_handle_t* handle) {
  ProcessorAsyncBegin* req =
      static_cast<ProcessorAsyncBegin*>(handle->data);
  handle->data = NULL;
}

void Processor::EmitProgress(uv_async_t* handle, int status) {
  assert(status == 0);
  ProcessorAsyncProgress* req =
      static_cast<ProcessorAsyncProgress*>(handle->data);

  req->processor->sink->EmitProgress(req->progress);

  NODE_ASYNC_CLOSE(handle, EmitProgressClose);
}

void Processor::EmitProgressClose(uv_handle_t* handle) {
  ProcessorAsyncProgress* req =
      static_cast<ProcessorAsyncProgress*>(handle->data);
  handle->data = NULL;
}

void Processor::EmitError(uv_async_t* handle, int status) {
  assert(status == 0);
  ProcessorAsyncError* req =
      static_cast<ProcessorAsyncError*>(handle->data);

  req->processor->sink->EmitError(req->err);

  NODE_ASYNC_CLOSE(handle, EmitErrorClose);
}

void Processor::EmitErrorClose(uv_handle_t* handle) {
  ProcessorAsyncError* req =
      static_cast<ProcessorAsyncError*>(handle->data);
  handle->data = NULL;
}

void Processor::EmitEnd(uv_async_t* handle, int status) {
  assert(status == 0);
  ProcessorAsyncEnd* req =
      static_cast<ProcessorAsyncEnd*>(handle->data);

  req->processor->sink->EmitEnd();

  NODE_ASYNC_CLOSE(handle, EmitEndClose);
}

void Processor::EmitEndClose(uv_handle_t* handle) {
  ProcessorAsyncEnd* req =
      static_cast<ProcessorAsyncEnd*>(handle->data);
  handle->data = NULL;
}

Progress Processor::GetProgress() {
  return this->progress;
}

void Processor::Abort() {
  pthread_mutex_lock(&this->lock);
  this->abort = true;
  pthread_mutex_unlock(&this->lock);
}
