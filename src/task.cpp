#include "task.h"
#include "mediainfo.h"

using namespace transcode;
using namespace v8;

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

  IOHandle* input = IOHandle::Create(task->source);
  IOHandle* output = IOHandle::Create(task->target);
  Profile* profile = new Profile(task->profile);
  task->context = new TaskContext(input, output, profile);

  // Kickoff
  task->Ref();
  // TODO: start thread

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

void Task::Complete() {
  assert(this->context);

  this->Unref();

  delete this->context;
  this->context = NULL;
}

TaskContext::TaskContext(IOHandle* input, IOHandle* output, Profile* profile) :
    running(true), abort(false),
    input(input), output(output), profile(profile),
    ictx(NULL), octx(NULL) {
  pthread_mutex_init(&this->lock, NULL);

  memset(&this->progress, 0, sizeof(this->progress));
}

TaskContext::~TaskContext() {
  assert(!this->running);

  pthread_mutex_destroy(&this->lock);

  if (this->ictx) {
    avformat_free_context(this->ictx);
  }
  if (this->octx) {
    avformat_free_context(this->octx);
  }

  delete this->input;
  delete this->output;
  delete this->profile;
}

Progress TaskContext::GetProgress() {
  pthread_mutex_lock(&this->lock);
  Progress progress = this->progress;
  pthread_mutex_unlock(&this->lock);
  return progress;
}

void TaskContext::Abort() {
  pthread_mutex_lock(&this->lock);
  this->abort = true;
  pthread_mutex_unlock(&this->lock);
}
