#include <node.h>
#include <v8.h>
#include "../utils.h"
#include "iohandle.h"

#ifndef NODE_TRANSCODING_IO_STREAMREADER
#define NODE_TRANSCODING_IO_STREAMREADER

using namespace v8;

namespace transcoding {
namespace io {

class StreamReader : public IOReader {
public:
  StreamReader(Handle<Object> source);
  virtual ~StreamReader();

  virtual int Open();
  virtual void Close();

private:
  static int ReadPacket(void* opaque, uint8_t* buffer, int bufferSize);
  static int64_t Seek(void* opaque, int64_t offset, int whence);

public:
  bool        canSeek;
};

}; // io
}; // transcoding

#endif // NODE_TRANSCODING_IO_STREAMREADER
