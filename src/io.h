#include <node.h>
#include <v8.h>
#include "utils.h"

#ifndef NODE_TRANSCODE_IO
#define NODE_TRANSCODE_IO

using namespace v8;

namespace transcode {

class IOHandle {
public:
  IOHandle(Handle<Object> source);
  virtual ~IOHandle();

  static IOHandle* Create(Handle<Object> source);

  virtual AVIOContext* openRead() = 0;
  virtual AVIOContext* openWrite() = 0;
  virtual void close(AVIOContext* s) = 0;

public:
  Persistent<Object>  source;
};

class FileHandle : public IOHandle {
public:
  FileHandle(Handle<Object> source);
  virtual ~FileHandle();

  virtual AVIOContext* openRead();
  virtual AVIOContext* openWrite();
  virtual void close(AVIOContext* s);

public:
  std::string     path;
};

class StreamHandle : public IOHandle {
public:
  StreamHandle(Handle<Object> source);
  virtual ~StreamHandle();

  virtual AVIOContext* openRead();
  virtual AVIOContext* openWrite();
  virtual void close(AVIOContext* s);

private:

  static int ReadPacket(void* opaque, uint8_t* buffer, int bufferSize);
  static int WritePacket(void* opaque, uint8_t* buffer, int bufferSize);
  static int64_t Seek(void* opaque, int64_t offset, int whence);

public:
  bool            canSeek;
};

class LiveStreamingHandle : public IOHandle {
public:
  LiveStreamingHandle(Handle<Object> source);
  virtual ~LiveStreamingHandle();

  // TODO: open segment/etc
  //FileHandle* createSegment(std::string name);

public:
  std::string     path;
};

AVFormatContext* createInputContext(IOHandle* input, int* pret);
AVFormatContext* createOutputContext(IOHandle* output, int* pret);

}; // transcode

#endif // NODE_TRANSCODE_IO
