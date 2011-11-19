#include <node.h>
#include <v8.h>
#include "utils.h"

#ifndef NODE_TRANSCODE_IO
#define NODE_TRANSCODE_IO

using namespace v8;

namespace transcode {

AVFormatContext* createInputContext(Handle<Object> source, int* pret);
AVFormatContext* createOutputContext(Handle<Object> target, int* pret);
void cleanupContext(AVFormatContext* ctx);

}; // transcode

#endif // NODE_TRANSCODE_IO
