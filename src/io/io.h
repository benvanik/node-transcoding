#include <node.h>
#include <v8.h>
#include "../utils.h"
#include "iohandle.h"

#ifndef NODE_TRANSCODING_IO
#define NODE_TRANSCODING_IO

using namespace v8;

namespace transcoding {
namespace io {

AVFormatContext* createInputContext(IOReader* input, int* pret);
AVFormatContext* createOutputContext(IOWriter* output, int* pret);

}; // io
}; // transcoding

#endif // NODE_TRANSCODING_IO
