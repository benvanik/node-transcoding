#include "query.h"
#include "mediainfo.h"

using namespace transcoding;
using namespace transcoding::io;

typedef struct QueryAsyncRequest_t {
  uv_async_t      req;
  Query*          query;
} QueryAsyncRequest;

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

Handle<Value> Query::New(const Arguments& args)
{
  HandleScope scope;
  Local<Object> source = args[0]->ToObject();
  Query* query = new Query(source);
  query->Wrap(args.This());
  return scope.Close(args.This());
}

Query::Query(Handle<Object> source) :
    context(NULL) {
  HandleScope scope;

  this->source = Persistent<Object>::New(source);
}

Query::~Query() {
  assert(!this->context);

  this->source.Dispose();
}

Handle<Value> Query::GetSource(Local<String> property,
    const AccessorInfo& info) {
  Query* query = ObjectWrap::Unwrap<Query>(info.This());
  HandleScope scope;
  return scope.Close(query->source);
}

Handle<Value> Query::Start(const Arguments& args) {
  Query* query = ObjectWrap::Unwrap<Query>(args.This());
  HandleScope scope;

  assert(!query->context);

  // Setup context
  IOReader* input = IOReader::Create(query->source);
  QueryContext* context = new QueryContext(input);

  // Prepare thread request
  uv_work_t* req = new uv_work_t();
  req->data = query;
  query->Ref();
  query->context = context;
  context->running = true;

  // Start thread
  int status = uv_queue_work(uv_default_loop(), req, ThreadWorker, NULL);
  assert(status == 0);

  return scope.Close(Undefined());
}

Handle<Value> Query::Stop(const Arguments& args) {
  Query* query = ObjectWrap::Unwrap<Query>(args.This());
  HandleScope scope;

  if (query->context) {
    query->context->Abort();
  }

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

void Query::EmitCompleteAsync(uv_async_t* handle, int status) {
  assert(status == 0);
  QueryAsyncRequest* req = static_cast<QueryAsyncRequest*>(handle->data);
  Query* query = req->query;
  QueryContext* context = query->context;
  assert(context);

  assert(context->running);
  context->running = false;
  int err = context->err;

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

  NODE_ASYNC_CLOSE(handle, AsyncHandleClose);
}

void Query::AsyncHandleClose(uv_handle_t* handle) {
  QueryAsyncRequest* req = static_cast<QueryAsyncRequest*>(handle->data);
  delete req;
  handle->data = NULL;
}

void Query::ThreadWorker(uv_work_t* request) {
  Query* query = static_cast<Query*>(request->data);
  QueryContext* context = query->context;
  assert(context);
  assert(context->running);

  QueryAsyncRequest* asyncReq;

  int ret = context->Execute();
  if (ret) {
    context->err = ret;
  }

  // Complete
  // Note that we fire this instead of doing it in the worker complete so that
  // all progress events will get dispatched prior to this
  asyncReq = new QueryAsyncRequest();
  asyncReq->req.data = asyncReq;
  asyncReq->query = query;
  uv_async_init(uv_default_loop(), &asyncReq->req, EmitCompleteAsync);
  uv_async_send(&asyncReq->req);
}
