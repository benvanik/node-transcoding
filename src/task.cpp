#include "task.h"
#include "mediainfo.h"

using namespace transcode;
using namespace v8;

typedef struct TaskAsyncRequest_t {
  uv_async_t      req;
  Task*           task;
  union {
    Progress      progress;
  };
} TaskAsyncRequest;

static Persistent<String> _task_timestamp_symbol;
static Persistent<String> _task_duration_symbol;
static Persistent<String> _task_timeElapsed_symbol;
static Persistent<String> _task_timeEstimated_symbol;
static Persistent<String> _task_timeRemaining_symbol;
static Persistent<String> _task_timeMultiplier_symbol;

static Persistent<FunctionTemplate> _task_ctor;

void Task::Init(Handle<Object> target) {
  HandleScope scope;

  _task_timestamp_symbol = NODE_PSYMBOL("timestamp");
  _task_duration_symbol = NODE_PSYMBOL("duration");
  _task_timeElapsed_symbol = NODE_PSYMBOL("timeElapsed");
  _task_timeEstimated_symbol = NODE_PSYMBOL("timeEstimated");
  _task_timeRemaining_symbol = NODE_PSYMBOL("timeRemaining");
  _task_timeMultiplier_symbol = NODE_PSYMBOL("timeMultiplier");

  Local<FunctionTemplate> ctor = FunctionTemplate::New(New);
  ctor->InstanceTemplate()->SetInternalFieldCount(1);
  ctor->SetClassName(String::NewSymbol("Task"));

  NODE_SET_PROTOTYPE_METHOD(ctor, "start", Start);
  NODE_SET_PROTOTYPE_METHOD(ctor, "stop", Stop);

  NODE_SET_PROTOTYPE_ACCESSOR(ctor, "source", GetSource);
  NODE_SET_PROTOTYPE_ACCESSOR(ctor, "target", GetTarget);
  NODE_SET_PROTOTYPE_ACCESSOR(ctor, "profile", GetProfile);
  NODE_SET_PROTOTYPE_ACCESSOR(ctor, "options", GetOptions);
  NODE_SET_PROTOTYPE_ACCESSOR(ctor, "progress", GetProgress);

  _task_ctor = Persistent<FunctionTemplate>::New(ctor);
  target->Set(String::NewSymbol("Task"), _task_ctor->GetFunction());
}

Handle<Value> Task::New(const Arguments& args)
{
  HandleScope scope;
  Local<Object> source = args[0]->ToObject();
  Local<Object> target = args[1]->ToObject();
  Local<Object> profile = args[2]->ToObject();
  Local<Object> options = args[3]->ToObject();
  Task* task = new Task(source, target, profile, options);
  task->Wrap(args.This());
  return scope.Close(args.This());
}

Task::Task(Handle<Object> source, Handle<Object> target, Handle<Object> profile,
    Handle<Object> options) :
    context(NULL) {
  HandleScope scope;

  this->source = Persistent<Object>::New(source);
  this->target = Persistent<Object>::New(target);
  this->profile = Persistent<Object>::New(profile);
  this->options = Persistent<Object>::New(options);
}

Task::~Task() {
}

Handle<Value> Task::GetSource(Local<String> property,
    const AccessorInfo& info) {
  Task* task = ObjectWrap::Unwrap<Task>(info.This());
  HandleScope scope;
  return scope.Close(task->source);
}

Handle<Value> Task::GetTarget(Local<String> property,
    const AccessorInfo& info) {
  Task* task = ObjectWrap::Unwrap<Task>(info.This());
  HandleScope scope;
  return scope.Close(task->target);
}

Handle<Value> Task::GetProfile(Local<String> property,
    const AccessorInfo& info) {
  Task* task = ObjectWrap::Unwrap<Task>(info.This());
  HandleScope scope;
  return scope.Close(task->profile);
}

Handle<Value> Task::GetOptions(Local<String> property,
    const AccessorInfo& info) {
  Task* task = ObjectWrap::Unwrap<Task>(info.This());
  HandleScope scope;
  return scope.Close(task->options);
}

Handle<Value> Task::GetProgressInternal(Progress* progress) {
  HandleScope scope;

  Local<Object> result = Object::New();

  result->Set(_task_timestamp_symbol,
      Number::New(progress->timestamp));
  result->Set(_task_duration_symbol,
      Number::New(progress->duration));
  result->Set(_task_timeElapsed_symbol,
      Number::New(progress->timeElapsed));
  result->Set(_task_timeEstimated_symbol,
      Number::New(progress->timeEstimated));
  result->Set(_task_timeRemaining_symbol,
      Number::New(progress->timeRemaining));
  result->Set(_task_timeMultiplier_symbol,
      Number::New(progress->timeMultiplier));

  return scope.Close(result);
}

Handle<Value> Task::GetProgress(Local<String> property,
    const AccessorInfo& info) {
  Task* task = ObjectWrap::Unwrap<Task>(info.This());
  HandleScope scope;

  if (task->context) {
    Progress progress = task->context->GetProgress();
    return scope.Close(task->GetProgressInternal(&progress));
  } else {
    return scope.Close(Null());
  }
}

Handle<Value> Task::Start(const Arguments& args) {
  Task* task = ObjectWrap::Unwrap<Task>(args.This());
  HandleScope scope;

  assert(!task->context);

  // Setup context
  IOHandle* input = IOHandle::Create(task->source);
  IOHandle* output = IOHandle::Create(task->target);
  Profile* profile = new Profile(task->profile);
  TaskContext* context = new TaskContext(input, output, profile);

  // Prepare the input/output (done on the main thread to make things easier)
  int ret = context->Prepare();
  if (ret) {
    delete context;
    task->EmitError(ret);
    return scope.Close(Undefined());
  }
  //task->EmitBegin(context->ictx, context->octx);

  printf("pre launch\n");

  // Prepare thread request
  uv_work_t* req = new uv_work_t();
  req->data = task;
  task->Ref();
  task->context = context;

  // Start thread
  int status = uv_queue_work(uv_default_loop(), req, ThreadWorker, NULL);
  assert(status == 0);

  printf("post launch\n");

  return scope.Close(Undefined());
}

Handle<Value> Task::Stop(const Arguments& args) {
  Task* task = ObjectWrap::Unwrap<Task>(args.This());
  HandleScope scope;

  if (task->context) {
    task->context->Abort();
  }

  return scope.Close(Undefined());
}

void Task::EmitBegin(AVFormatContext* ictx, AVFormatContext* octx) {
  HandleScope scope;

  Local<Object> sourceInfo = Local<Object>::New(createMediaInfo(ictx, false));
  Local<Object> targetInfo = Local<Object>::New(createMediaInfo(octx, true));

  Handle<Value> argv[] = {
    String::New("begin"),
    sourceInfo,
    targetInfo,
  };
  node::MakeCallback(this->handle_, "emit", countof(argv), argv);
}

void Task::EmitProgress(Progress progress) {
  HandleScope scope;

  Handle<Value> argv[] = {
    String::New("progress"),
    this->GetProgressInternal(&progress),
  };
  node::MakeCallback(this->handle_, "emit", countof(argv), argv);
}

void Task::EmitError(int err) {
  HandleScope scope;

  char buffer[256];
  av_strerror(err, buffer, sizeof(buffer));

  Handle<Value> argv[] = {
    String::New("error"),
    Exception::Error(String::New(buffer)),
  };
  node::MakeCallback(this->handle_, "emit", countof(argv), argv);
}

void Task::EmitEnd() {
  HandleScope scope;

  Handle<Value> argv[] = {
    String::New("end"),
  };
  node::MakeCallback(this->handle_, "emit", countof(argv), argv);
}

void Task::EmitProgressAsync(uv_async_t* handle, int status) {
  assert(status == 0);
  TaskAsyncRequest* req = static_cast<TaskAsyncRequest*>(handle->data);

  req->task->EmitProgress(req->progress);

  NODE_ASYNC_CLOSE(handle, AsyncHandleClose);
}

void Task::EmitCompleteAsync(uv_async_t* handle, int status) {
  assert(status == 0);
  TaskAsyncRequest* req = static_cast<TaskAsyncRequest*>(handle->data);
  Task* task = req->task;
  TaskContext* context = task->context;
  assert(context);

  printf("complete\n");

  // Always fire one last progress event
  if (!context->err) {
    context->progress.timestamp = context->progress.duration;
    context->progress.timeRemaining = 0;
  }
  task->EmitProgress(context->progress);

  assert(context->running);
  context->running = false;
  int err = context->err;

  delete task->context;
  task->context = NULL;

  if (err) {
    task->EmitError(err);
  } else {
    task->EmitEnd();
  }

  task->Unref();

  NODE_ASYNC_CLOSE(handle, AsyncHandleClose);
}

void Task::AsyncHandleClose(uv_handle_t* handle) {
  TaskAsyncRequest* req = static_cast<TaskAsyncRequest*>(handle->data);
  delete req;
  handle->data = NULL;
}

void Task::ThreadWorker(uv_work_t* request) {
  Task* task = static_cast<Task*>(request->data);
  TaskContext* context = task->context;
  assert(context);

  int64_t startTime = av_gettime();
  int64_t lastProgressTime = 0;
  Progress progress;
  memset(&progress, 0, sizeof(progress));
  //progress.duration   = context->ictx->duration / (double)AV_TIME_BASE;
  double percentDelta = 0;

  printf("PRE\n");

  TaskAsyncRequest* asyncReq;
  int ret = 0;
  bool aborting = false;
  do {
    // Copy progress (speeds up queries later on)
    // Also grab the current abort flag
    pthread_mutex_lock(&context->lock);
    aborting = context->abort;
    memcpy(&context->progress, &progress, sizeof(progress));
    pthread_mutex_unlock(&context->lock);

    // Emit progress event, if needed
    int64_t currentTime = av_gettime();
    bool emitProgress =
        (currentTime - lastProgressTime > 1 * 1000000) ||
        (percentDelta > 0.01);
    if (emitProgress) {
      lastProgressTime = currentTime;
      percentDelta = 0;

      asyncReq = new TaskAsyncRequest();
      asyncReq->req.data = asyncReq;
      asyncReq->task = task;
      asyncReq->progress = context->progress;
      uv_async_init(uv_default_loop(), &asyncReq->req, EmitProgressAsync);
      uv_async_send(&asyncReq->req);
    }

    // Perform some work
    printf("PUMP->\n");
    bool finished = context->Pump(&ret);
    printf("->PUMP\n");

    if (finished) {
      // ?
      break;
    }
  } while (!ret && !aborting);

  // Complete
  // Note that we fire this instead of doing it in the worker complete so that
  // all progress events will get dispatched prior to this
  asyncReq = new TaskAsyncRequest();
  asyncReq->req.data = asyncReq;
  asyncReq->task = task;
  asyncReq->progress = context->progress;
  uv_async_init(uv_default_loop(), &asyncReq->req, EmitCompleteAsync);
  uv_async_send(&asyncReq->req);

  printf("POST %d\n", context->err);
}
