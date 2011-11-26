#include <node.h>
#include <v8.h>
#include "../utils.h"
#include "iohandle.h"

#ifndef NODE_TRANSCODING_IO_STREAMWRITER
#define NODE_TRANSCODING_IO_STREAMWRITER

using namespace v8;

namespace transcoding {
namespace io {

#define STREAMWRITER_BUFFER_SIZE  (64 * 1024)
#define STREAMWRITER_MAX_SIZE     (64 * 1024 * 1024)

class StreamWriter : public IOWriter {
public:
  StreamWriter(Handle<Object> source,
      size_t maxBufferedBytes = STREAMWRITER_MAX_SIZE);
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
