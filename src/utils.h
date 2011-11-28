#include <node.h>
#include <v8.h>
#include <fcntl.h>
#include <string>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

#ifndef NODE_TRANSCODING_UTILS
#define NODE_TRANSCODING_UTILS

namespace transcoding {

// Set to 1 to enable massive debug spew
#define TC_LOG_DEBUG 1

#if TC_LOG_DEBUG
#define TC_LOG_D(args...) printf(args)
#else
#define TC_LOG_D(msg, ...)
#endif // TC_LOG_DEBUG

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

#define NODE_ON_EVENT(obj, name, inst, callback, target) \
  do { \
    Local<FunctionTemplate> __cbt = FunctionTemplate::New(callback, \
        External::New(reinterpret_cast<void*>(target))); \
    Local<Function> __cb = __cbt->GetFunction(); \
    __cb->SetName(String::New(name)); \
    inst = Persistent<Function>::New(__cb); \
    Local<Function> __on = Local<Function>::Cast(obj->Get(String::New("on"))); \
    __on->Call(obj, 2, (Handle<Value>[]){ String::New(name), __cb }); \
  } while(0)

#define NODE_REMOVE_EVENT(obj, name, inst) \
  do { \
    Local<Function> __removeListener = \
        Local<Function>::Cast(obj->Get(String::New("removeListener"))); \
    __removeListener->Call(obj, \
        2, (Handle<Value>[]){ String::New(name), inst }); \
    inst.Dispose(); \
    inst.Clear(); \
  } while(0)

static std::string V8GetString(v8::Handle<v8::Object> obj, const char* name,
    std::string original) {
  v8::HandleScope scope;
  v8::Local<v8::String> value =
      v8::Local<v8::String>::Cast(obj->Get(v8::String::NewSymbol(name)));
  if (value.IsEmpty()) {
    return original;
  } else {
    return *v8::String::Utf8Value(value);
  }
}

static double V8GetNumber(v8::Handle<v8::Object> obj, const char* name,
    double original) {
  v8::HandleScope scope;
  v8::Local<v8::Object> value =
      v8::Local<v8::Object>::Cast(obj->Get(v8::String::NewSymbol(name)));
  if (value.IsEmpty()) {
    return original;
  } else {
    return value->NumberValue();
  }
}

static bool V8GetBoolean(v8::Handle<v8::Object> obj, const char* name,
    bool original) {
  v8::HandleScope scope;
  v8::Local<v8::Object> value =
      v8::Local<v8::Object>::Cast(obj->Get(v8::String::NewSymbol(name)));
  if (value.IsEmpty()) {
    return original;
  } else {
    return value->IsTrue();
  }
}

}; // transcoding

#endif // NODE_TRANSCODING_UTILS
