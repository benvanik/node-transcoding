#include <node.h>
#include <v8.h>
#include "utils.h"

#ifndef NODE_TRANSCODING_IO
#define NODE_TRANSCODING_IO

using namespace v8;

namespace transcoding {

class IOHandle {
public:
  IOHandle(Handle<Object> source);
  virtual ~IOHandle();

  static IOHandle* Create(Handle<Object> source);

  virtual AVIOContext* OpenRead() = 0;
  virtual AVIOContext* OpenWrite() = 0;
  virtual void Close(AVIOContext* s) = 0;

public:
  Persistent<Object>  source;
};

class FileHandle : public IOHandle {
public:
  FileHandle(Handle<Object> source);
  virtual ~FileHandle();

  virtual AVIOContext* OpenRead();
  virtual AVIOContext* OpenWrite();
  virtual void Close(AVIOContext* s);

public:
  std::string     path;
};

class StreamHandle : public IOHandle {
public:
  StreamHandle(Handle<Object> source);
  virtual ~StreamHandle();

  virtual AVIOContext* OpenRead();
  virtual AVIOContext* OpenWrite();
  virtual void Close(AVIOContext* s);

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

}; // transcoding

#endif // NODE_TRANSCODING_IO
