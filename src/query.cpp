#include "query.h"
#include "mediainfo.h"

using namespace transcoding;
using namespace transcoding::io;

static Persistent<FunctionTemplate> _query_ctor;

void Query::Init(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> ctor = FunctionTemplate::New(New);
  ctor->InstanceTemplate()->SetInternalFieldCount(1);
  ctor->SetClassName(String::NewSymbol("Query"));

  NODE_SET_PROTOTYPE_METHOD(ctor, "start", Start);
  NODE_SET_PROTOTYPE_METHOD(ctor, "stop", Stop);

  NODE_SET_PROTOTYPE_ACCESSOR(ctor, "source", GetSource);

  _query_ctor = Persistent<FunctionTemplate>::New(ctor);
  target->Set(String::NewSymbol("Query"), _query_ctor->GetFunction());
}

Handle<Value> Query::New(const Arguments& args) {
  HandleScope scope;
  Local<Object> source = args[0]->ToObject();
  Query* query = new Query(source);
  query->Wrap(args.This());
  return scope.Close(args.This());
}

Query::Query(Handle<Object> source) :
    context(NULL), abort(false), err(0) {
  TC_LOG_D("Query::Query()\n");
  HandleScope scope;

  this->source = Persistent<Object>::New(source);

  pthread_mutex_init(&this->lock, NULL);

  this->asyncReq = new uv_async_t();
  this->asyncReq->data = this;
  uv_async_init(uv_default_loop(), this->asyncReq, CompleteAsync);
}

Query::~Query() {
  TC_LOG_D("Query::~Query()\n");
  HandleScope scope;
  assert(!this->context);

  pthread_mutex_destroy(&this->lock);

  this->source.Dispose();
}

Handle<Value> Query::GetSource(Local<String> property,
    const AccessorInfo& info) {
  HandleScope scope;
  Query* query = ObjectWrap::Unwrap<Query>(info.This());
  return scope.Close(query->source);
}

Handle<Value> Query::Start(const Arguments& args) {
  TC_LOG_D("Query::Start()\n");

  Query* query = ObjectWrap::Unwrap<Query>(args.This());
  HandleScope scope;

  assert(!query->context);

  // Since we are just a query, only read small chunks - if sourcing from the
  // network this will greatly reduce the chance of use downloading entire files
  // just to read the headers
  size_t maxBufferBytes = 128 * 1024;
  IOReader* input = IOReader::Create(query->source, maxBufferBytes);

  QueryContext* context = new QueryContext(input);

  // Prepare thread request
  uv_work_t* req = new uv_work_t();
  req->data = query;
  query->Ref();
  query->context = context;

  // Start thread
  int status = uv_queue_work(uv_default_loop(), req,
      ThreadWorker, ThreadWorkerAfter);
  assert(status == 0);

  return scope.Close(Undefined());
}

Handle<Value> Query::Stop(const Arguments& args) {
  TC_LOG_D("Query::Stop()\n");

  HandleScope scope;
  Query* query = ObjectWrap::Unwrap<Query>(args.This());

  pthread_mutex_lock(&query->lock);
  query->abort = true;
  pthread_mutex_unlock(&query->lock);

  return scope.Close(Undefined());
}

void Query::EmitInfo(Handle<Object> sourceInfo) {
  HandleScope scope;

  Handle<Value> argv[] = {
    String::New("info"),
    sourceInfo,
  };
  node::MakeCallback(this->handle_, "emit", countof(argv), argv);
}

void Query::EmitError(int err) {
  HandleScope scope;

  char buffer[256];
  av_strerror(err, buffer, sizeof(buffer));

  Handle<Value> argv[] = {
    String::New("error"),
    Exception::Error(String::New(buffer)),
  };
  node::MakeCallback(this->handle_, "emit", countof(argv), argv);
}

void Query::CompleteAsync(uv_async_t* handle, int status) {
  HandleScope scope;
  Query* query = static_cast<Query*>(handle->data);
  if (!query) {
    return;
  }
  TC_LOG_D("Query::CompleteAsync(): err %d\n", query->err);
  QueryContext* context = query->context;
  assert(context);

  int err = query->err;

  Local<Object> sourceInfo;
  if (!err) {
    sourceInfo = Local<Object>::New(createMediaInfo(context->ictx, false));
  }

  delete query->context;
  query->context = NULL;

  if (err) {
    query->EmitError(err);
  } else {
    query->EmitInfo(sourceInfo);
  }

  query->Unref();
  handle->data = NULL; // no more

  uv_close((uv_handle_t*)handle, AsyncHandleClose);
}

void Query::AsyncHandleClose(uv_handle_t* handle) {
  TC_LOG_D("Query::AsyncHandleClose()\n");
  delete handle;
}

void Query::ThreadWorker(uv_work_t* request) {
  TC_LOG_D("Query::ThreadWorker()\n");

  Query* query = static_cast<Query*>(request->data);
  QueryContext* context = query->context;
  assert(context);

  int ret = context->Execute();
  if (ret) {
    TC_LOG_D("Query::ThreadWorker(): execute failed (%d)\n", ret);
    query->err = ret;
  }

  // Complete
  // Note that we fire this instead of doing it in the worker complete so that
  // all progress events will get dispatched prior to this
  uv_async_send(query->asyncReq);

  TC_LOG_D("Query::ThreadWorker() exiting\n");
}

void Query::ThreadWorkerAfter(uv_work_t* request) {
  delete request;
}
