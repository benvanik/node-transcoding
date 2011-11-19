#include <node.h>
#include <v8.h>

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

}; // transcode

#endif // NODE_TRANSCODE_UTILS
