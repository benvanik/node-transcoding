#include <node.h>
#include <v8.h>
#include <node_buffer.h>
#include <vector>
#include "../utils.h"
#include "iohandle.h"

#ifndef NODE_TRANSCODING_IO_STREAMREADER
#define NODE_TRANSCODING_IO_STREAMREADER

using namespace v8;

namespace transcoding {
namespace io {

#define STREAMREADER_BUFFER_SIZE  (64 * 1024)
#define STREAMREADER_MAX_SIZE     (1 * 1024 * 1024)

class ReadBuffer {
public:
  ReadBuffer(uint8_t* source, int64_t length);
  ~ReadBuffer();

  bool IsEmpty();
  int64_t Read(uint8_t* buffer, int64_t bufferSize);

public:
  int64_t       offset;
  uint8_t*      data;
  int64_t       length;
};

class StreamReader : public IOReader {
public:
  StreamReader(Handle<Object> source,
      size_t maxBufferedBytes = STREAMREADER_MAX_SIZE);
  virtual ~StreamReader();

  virtual int Open();
  virtual void Close();

private:
  static Handle<Value> OnData(const Arguments& args);
  static Handle<Value> OnEnd(const Arguments& args);
  static Handle<Value> OnClose(const Arguments& args);
  static Handle<Value> OnError(const Arguments& args);

  static void ResumeAsync(uv_async_t* handle, int status);
  static void AsyncHandleClose(uv_handle_t* handle);

  static int ReadPacket(void* opaque, uint8_t* buffer, int bufferSize);
  static int64_t Seek(void* opaque, int64_t offset, int whence);

public:
  bool        canSeek;

  Persistent<Function>  sourcePause;
  Persistent<Function>  sourceResume;

  Persistent<Function>  onData;
  Persistent<Function>  onEnd;
  Persistent<Function>  onClose;
  Persistent<Function>  onError;

  uv_async_t*           asyncReq;

  pthread_mutex_t       lock;
  pthread_cond_t        cond;
  bool                  paused;
  int                   err;
  bool                  eof;
  int64_t               maxBufferedBytes;
  int64_t               totalBufferredBytes;
  std::vector<ReadBuffer*> buffers;
};

}; // io
}; // transcoding

#endif // NODE_TRANSCODING_IO_STREAMREADER
