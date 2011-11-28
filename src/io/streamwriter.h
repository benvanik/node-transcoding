#include <node.h>
#include <v8.h>
#include <node_buffer.h>
#include <vector>
#include "../utils.h"
#include "iohandle.h"

#ifndef NODE_TRANSCODING_IO_STREAMWRITER
#define NODE_TRANSCODING_IO_STREAMWRITER

using namespace v8;

namespace transcoding {
namespace io {

#define STREAMWRITER_BUFFER_SIZE  (64 * 1024)
#define STREAMWRITER_MAX_SIZE     (32 * 1024 * 1024)

class WriteBuffer {
public:
  WriteBuffer(uint8_t* source, int64_t length);
  ~WriteBuffer();

  void Steal();

public:
  uint8_t*      data;
  int64_t       length;
};

class StreamWriter : public IOWriter {
public:
  StreamWriter(Handle<Object> source,
      size_t maxBufferedBytes = STREAMWRITER_MAX_SIZE);
  virtual ~StreamWriter();

  virtual int Open();
  virtual void Close();

  virtual bool QueueCloseOnIdle();

private:
  static Handle<Value> OnDrain(const Arguments& args);
  static Handle<Value> OnClose(const Arguments& args);
  static Handle<Value> OnError(const Arguments& args);

  static void IdleCallback(uv_idle_t* handle, int status);
  static void IdleHandleClose(uv_handle_t* handle);

  static int WritePacket(void* opaque, uint8_t* buffer, int bufferSize);
  static int64_t Seek(void* opaque, int64_t offset, int whence);

public:
  bool        canSeek;

  Persistent<Function>  onDrain;
  Persistent<Function>  onClose;
  Persistent<Function>  onError;

  uv_idle_t*            idle;

  pthread_mutex_t       lock;
  pthread_cond_t        cond;
  bool                  kernelBufferFull;
  bool                  selfBufferFull;
  int                   err;
  bool                  eof;
  bool                  closing;
  int64_t               maxBufferedBytes;
  int64_t               totalBufferredBytes;
  std::vector<WriteBuffer*> buffers;
};

}; // io
}; // transcoding

#endif // NODE_TRANSCODING_IO_STREAMWRITER
