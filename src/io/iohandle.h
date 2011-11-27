#include <node.h>
#include <v8.h>
#include "../utils.h"

#ifndef NODE_TRANSCODING_IO_IOHANDLE
#define NODE_TRANSCODING_IO_IOHANDLE

using namespace v8;

namespace transcoding {
namespace io {

class IOHandle {
public:
  IOHandle(Handle<Object> source);
  virtual ~IOHandle();

  virtual int Open() = 0;
  virtual void Close() = 0;

  static void CloseWhenDone(IOHandle* handle);
  virtual bool QueueCloseOnIdle();

public:
  Persistent<Object>  source;
  AVIOContext*        context;
};

class IOReader : public IOHandle {
public:
  IOReader(Handle<Object> source);
  virtual ~IOReader();

  static IOReader* Create(Handle<Object> source, size_t maxBufferedBytes = 0);
};

class IOWriter : public IOHandle {
public:
  IOWriter(Handle<Object> source);
  virtual ~IOWriter();

  static IOWriter* Create(Handle<Object> source, size_t maxBufferedBytes = 0);
};

}; // io
}; // transcoding

#endif // NODE_TRANSCODING_IO_IOHANDLE
