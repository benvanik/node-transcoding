#include <node.h>
#include <v8.h>
#include "utils.h"
#include "querycontext.h"
#include "io/io.h"

#ifndef NODE_TRANSCODING_QUERY
#define NODE_TRANSCODING_QUERY

using namespace v8;

namespace transcoding {

class Query : public node::ObjectWrap {
public:
  static void Init(Handle<Object> target);
  static Handle<Value> New(const Arguments& args);

public:
  Query(Handle<Object> source);
  ~Query();

  static Handle<Value> GetSource(Local<String> property,
      const AccessorInfo& info);

  static Handle<Value> Start(const Arguments& args);
  static Handle<Value> Stop(const Arguments& args);

public:
  void EmitInfo(Handle<Object> sourceInfo);
  void EmitError(int err);

  static void CompleteAsync(uv_async_t* handle, int status);
  static void AsyncHandleClose(uv_handle_t* handle);

  static void ThreadWorker(uv_work_t* request);
  static void ThreadWorkerAfter(uv_work_t* request);

private:
  Persistent<Object>  source;

  QueryContext*       context;

  pthread_mutex_t     lock;
  bool                abort;
  int                 err;

  uv_async_t*         asyncReq;
};

}; // transcoding

#endif // NODE_TRANSCODING_QUERY
