#include "task.h"
#include "mediainfo.h"

using namespace transcoding;
using namespace transcoding::io;

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

Handle<Value> Task::New(const Arguments& args) {
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
    context(NULL), abort(false), err(0) {
  TC_LOG_D("Task::Task()\n");
  HandleScope scope;

  this->source = Persistent<Object>::New(source);
  this->target = Persistent<Object>::New(target);
  this->profile = Persistent<Object>::New(profile);
  this->options = Persistent<Object>::New(options);

  memset(&this->progress, 0, sizeof(this->progress));

  this->asyncReq = new uv_async_t();
  this->asyncReq->data = this;
  uv_async_init(uv_default_loop(), this->asyncReq, ProcessAsync);

  pthread_mutex_init(&this->lock, NULL);
}

Task::~Task() {
  TC_LOG_D("Task::~Task()\n");
  assert(!this->context);

  if (this->messages.size()) {
    TC_LOG_D("Task::~Task(): dtor when messages %d pending\n",
        (int)this->messages.size());
  }
  pthread_mutex_destroy(&this->lock);

  this->source.Dispose();
  this->target.Dispose();
  this->profile.Dispose();
  this->options.Dispose();
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
    return scope.Close(task->GetProgressInternal(&task->progress));
  } else {
    return scope.Close(Null());
  }
}

Handle<Value> Task::Start(const Arguments& args) {
  TC_LOG_D("Task::Start()\n");
  Task* task = ObjectWrap::Unwrap<Task>(args.This());
  HandleScope scope;

  assert(!task->context);

  // Setup context
  IOReader* input = IOReader::Create(task->source);
  IOWriter* output = IOWriter::Create(task->target);
  Profile* profile = new Profile(task->profile);
  TaskContext* context = new TaskContext(input, output, profile);

  // Prepare thread request
  uv_work_t* req = new uv_work_t();
  req->data = task;
  task->Ref();
  task->context = context;

  // Start thread
  int status = uv_queue_work(uv_default_loop(), req,
      ThreadWorker, ThreadWorkerAfter);
  assert(status == 0);

  return scope.Close(Undefined());
}

Handle<Value> Task::Stop(const Arguments& args) {
  TC_LOG_D("Task::Stop()\n");
  Task* task = ObjectWrap::Unwrap<Task>(args.This());
  HandleScope scope;

  pthread_mutex_lock(&task->lock);
  task->abort = true;
  pthread_mutex_unlock(&task->lock);

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

void Task::ProcessAsync(uv_async_t* handle, int status) {
  assert(status == 0);
  Task* task = static_cast<Task*>(handle->data);
  if (!task) {
    return;
  }

  TaskContext* context = task->context;
  assert(context);

  while (true) {
    int abortAll = false;

    pthread_mutex_lock(&task->lock);
    TaskMessage* message = NULL;
    int remaining = task->messages.size();
    if (remaining) {
      message = task->messages.front();
      task->messages.erase(task->messages.begin());
    }
    pthread_mutex_unlock(&task->lock);
    if (!message) {
      break;
    }

    switch (message->type) {
      case TaskMessageBegin:
        TC_LOG_D("Task::ProcessAsync(TaskMessageBegin)\n");
        // NOTE: NOT THREAD SAFE AGHHHH
        task->EmitBegin(context->ictx, context->octx);
        break;
      case TaskMessageProgress:
        TC_LOG_D("Task::ProcessAsync(TaskMessageProgress)\n");
        task->progress = message->progress;
        task->EmitProgress(message->progress);
        break;
      case TaskMessageComplete:
      TC_LOG_D("Task::ProcessAsync(TaskMessageComplete), err: %d\n", task->err);
        // Always fire one last progress event
        if (!task->err) {
          task->progress.timestamp = task->progress.duration;
          task->progress.timeRemaining = 0;
        }
        task->EmitProgress(task->progress);

        int err = task->err;

        delete task->context;
        task->context = NULL;

        if (err) {
          task->EmitError(err);
        } else {
          task->EmitEnd();
        }

        uv_close((uv_handle_t*)task->asyncReq, AsyncHandleClose);

        task->Unref();
        handle->data = NULL; // no more processing!
        abortAll = true;
        break;
    }

    delete message;
  }
}

void Task::AsyncHandleClose(uv_handle_t* handle) {
  delete handle;
}

#define EMIT_PROGRESS_TIME_CAP    1.0   // sec between emits
#define EMIT_PROGRESS_PERCENT_CAP 0.01  // 1/100*% between emits

void Task::ThreadWorker(uv_work_t* request) {
  TC_LOG_D("Task::ThreadWorker()\n");

  Task* task = static_cast<Task*>(request->data);
  TaskContext* context = task->context;
  assert(context);

  TaskMessage* msg;

  // Prepare the input/output (done on the main thread to make things easier)
  int ret = context->Prepare();
  if (ret) {
    TC_LOG_D("Task::ThreadWorker(): Prepare failed (%d)\n", ret);
    pthread_mutex_lock(&task->lock);
    task->err = ret;
    task->messages.push_back(new TaskMessage(TaskMessageComplete));
    pthread_mutex_unlock(&task->lock);
    uv_async_send(task->asyncReq);
    return;
  }

  // Begin
  pthread_mutex_lock(&task->lock);
  task->messages.push_back(new TaskMessage(TaskMessageBegin));
  pthread_mutex_unlock(&task->lock);
  uv_async_send(task->asyncReq);

  double percentDelta = 0;
  int64_t startTime = av_gettime();
  int64_t lastProgressTime = 0;
  Progress progress;
  memset(&progress, 0, sizeof(progress));
  progress.duration   = context->ictx->duration / (double)AV_TIME_BASE;

  bool aborting = false;
  do {
    // Grab the current abort flag
    pthread_mutex_lock(&task->lock);
    aborting = task->abort;
    pthread_mutex_unlock(&task->lock);
    if (aborting) {
      TC_LOG_D("Task::ThreadWorker(): aborting\n");
      break;
    }

    // Emit progress event, if needed
    int64_t currentTime = av_gettime();
    bool emitProgress =
        (currentTime - lastProgressTime > EMIT_PROGRESS_TIME_CAP * 1000000) ||
        (percentDelta > EMIT_PROGRESS_PERCENT_CAP);
    if (emitProgress) {
      lastProgressTime = currentTime;
      percentDelta = 0;

      // Progress
      TaskMessage* msg = new TaskMessage(TaskMessageProgress);
      msg->progress = progress;
      pthread_mutex_lock(&task->lock);
      task->messages.push_back(msg);
      // Quick optimization so we don't flood requests
      bool needsAsyncReq = task->messages.size() == 1;
      pthread_mutex_unlock(&task->lock);
      if (needsAsyncReq) {
        uv_async_send(task->asyncReq);
      }
    }

    // Perform some work
    double oldPercent = progress.timestamp / progress.duration;
    bool finished = context->Pump(&ret, &progress);
    percentDelta += (progress.timestamp / progress.duration) - oldPercent;
    task->err = ret;

    // End, if needed
    if (finished && !ret) {
      TC_LOG_D("Task::ThreadWorker(): end, err: %d, ret: %d, finished: %d\n",
          task->err, ret, finished);
      context->End();
      break;
    }
  } while (!ret && !aborting);

  TC_LOG_D("Task::ThreadWorker(): done (%d)\n", task->err);

  // Complete
  // Note that we fire this instead of doing it in the worker complete so that
  // all progress events will get dispatched prior to this
  pthread_mutex_lock(&task->lock);
  task->messages.push_back(new TaskMessage(TaskMessageComplete));
  pthread_mutex_unlock(&task->lock);
  uv_async_send(task->asyncReq);

  TC_LOG_D("Task::ThreadWorker() exiting\n");
}

void Task::ThreadWorkerAfter(uv_work_t* request) {
  delete request;
}
