#include <node.h>
#include <v8.h>
#include "utils.h"
#include "task.h"

using namespace transcode;
using namespace v8;

namespace transcode {

Handle<Value> queryInfo(const Arguments& args) {
  HandleScope scope;

  //

  return scope.Close(Undefined());
}

}; // transcode

extern "C" void node_transcode_init(Handle<Object> target) {
  HandleScope scope;

  transcode::Task::Init(target);

  target->Set(String::NewSymbol("queryInfo"),
      FunctionTemplate::New(transcode::queryInfo)->GetFunction());
}

NODE_MODULE(node_transcode, node_transcode_init);
