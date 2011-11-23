#include <node.h>
#include <v8.h>
#include "utils.h"

#ifndef NODE_TRANSCODING_MEDIAINFO
#define NODE_TRANSCODING_MEDIAINFO

using namespace v8;

namespace transcoding {

Handle<Object> createMediaInfo(AVFormatContext* ctx, bool encoding);

}; // transcoding

#endif // NODE_TRANSCODING_MEDIAINFO
