#include <node.h>
#include <v8.h>
#include "utils.h"

#ifndef NODE_TRANSCODE_MEDIAINFO
#define NODE_TRANSCODE_MEDIAINFO

using namespace v8;

namespace transcode {

Handle<Object> createMediaInfo(AVFormatContext* ctx, bool encoding);

}; // transcode

#endif // NODE_TRANSCODE_MEDIAINFO
