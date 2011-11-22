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
  Local<Object> options = args[2]->ToObject();
  Task* task = new Task(source, target, options);
  task->Wrap(args.This());
  return scope.Close(args.This());
}

Task::Task(
    Handle<Object> source, Handle<Object> target, Handle<Object> options) {
  HandleScope scope;

  this->source = Persistent<Object>::New(source);
  this->target = Persistent<Object>::New(target);
  this->options = Persistent<Object>::New(options);

  // Not retained - lifetime is tied to this instance
  this->processor.SetSink(this);
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

  Progress progress = task->processor.GetProgress();

  return scope.Close(task->GetProgressInternal(&progress));
}

Handle<Value> Task::Start(const Arguments& args) {
  Task* task = ObjectWrap::Unwrap<Task>(args.This());
  HandleScope scope;

  InputDescriptor* input = new InputDescriptor(task->source);
  OutputDescriptor* output = new OutputDescriptor(task->target);
  ProcessorOptions* options = new ProcessorOptions(task->options);
  task->processor.Execute(input, output, options, args.This());

  return scope.Close(Undefined());
}

Handle<Value> Task::Stop(const Arguments& args) {
  Task* task = ObjectWrap::Unwrap<Task>(args.This());
  HandleScope scope;

  task->processor.Abort();

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
