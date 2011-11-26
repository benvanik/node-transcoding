#include <node.h>
#include <v8.h>
#include "../utils.h"
#include "iohandle.h"

#ifndef NODE_TRANSCODING_IO_STREAMWRITER
#define NODE_TRANSCODING_IO_STREAMWRITER

using namespace v8;

namespace transcoding {
namespace io {

class StreamWriter : public IOWriter {
public:
  StreamWriter(Handle<Object> source);
  virtual ~StreamWriter();

  virtual int Open();
  virtual void Close();

private:
  static int WritePacket(void* opaque, uint8_t* buffer, int bufferSize);
  static int64_t Seek(void* opaque, int64_t offset, int whence);

public:
  bool        canSeek;
};

}; // io
}; // transcoding

#endif // NODE_TRANSCODING_IO_STREAMWRITER
