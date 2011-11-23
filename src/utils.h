#include <node.h>
#include <v8.h>
#include <string>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

#ifndef NODE_TRANSCODING_UTILS
#define NODE_TRANSCODING_UTILS

namespace transcoding {

#ifndef countof
#ifdef _countof
#define countof _countof
#else
#define countof(a) (sizeof(a) / sizeof(*(a)))
#endif
#endif

#define NODE_SET_PROTOTYPE_ACCESSOR(templ, name, callback)                \
do {                                                                      \
  templ->PrototypeTemplate()->SetAccessor(v8::String::NewSymbol(name),    \
                                          callback);                      \
} while (0)

#define NODE_ASYNC_SEND(req, name) \
  do { \
    uv_async_t* handle_##name = new uv_async_t; \
    handle_##name->data = req; \
    uv_async_init(uv_default_loop(), handle_##name, name); \
    uv_async_send(handle_##name); \
  } while(0);

#define NODE_ASYNC_CLOSE(handle, name) \
  do { \
    uv_close((uv_handle_t*)handle, name); \
  } while(0);

static std::string V8GetString(v8::Handle<v8::Object> obj, const char* name,
    std::string& original) {
  v8::HandleScope scope;
  v8::Local<v8::String> value =
      v8::Local<v8::String>::Cast(obj->Get(v8::String::NewSymbol(name)));
  if (value.IsEmpty()) {
    return original;
  } else {
    return *v8::String::AsciiValue(value);
  }
}

static double V8GetNumber(v8::Handle<v8::Object> obj, const char* name,
    double original) {
  v8::HandleScope scope;
  v8::Local<v8::Number> value =
      v8::Local<v8::Number>::Cast(obj->Get(v8::String::NewSymbol(name)));
  if (value.IsEmpty()) {
    return original;
  } else {
    return value->Value();
  }
}

}; // transcoding

#endif // NODE_TRANSCODING_UTILS
