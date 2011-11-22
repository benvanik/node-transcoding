#include <node.h>
#include <v8.h>
#include <string>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

#ifndef NODE_TRANSCODE_UTILS
#define NODE_TRANSCODE_UTILS

namespace transcode {

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

}; // transcode

#endif // NODE_TRANSCODE_UTILS
