#include "streamreader.h"
#include <node_buffer.h>

using namespace node;
using namespace transcoding;
using namespace transcoding::io;

#define STREAM_HANDLE_BUFFER_SIZE (64 * 1024)

StreamReader::StreamReader(Handle<Object> source) :
    IOReader(source) {
  HandleScope scope;

  // TODO: support seeking?
  this->canSeek = false;
}

StreamReader::~StreamReader() {
}

int StreamReader::Open() {
  int bufferSize = STREAM_HANDLE_BUFFER_SIZE;
  uint8_t* buffer = (uint8_t*)av_malloc(bufferSize);
  AVIOContext* s = avio_alloc_context(
      buffer, bufferSize,
      0, // 1 = write
      this,
      ReadPacket, NULL, this->canSeek ? Seek : NULL);
  this->context = s;
  return 0;
}

void StreamReader::Close() {
  HandleScope scope;
  Local<Object> global = Context::GetCurrent()->Global();

  Local<Function> end =
      Local<Function>::Cast(this->source->Get(String::New("end")));
  end->Call(this->source, 0, NULL);

  av_free(this->context);
  this->context = NULL;
}

int StreamReader::ReadPacket(void* opaque, uint8_t* buffer, int bufferSize) {
  StreamReader* stream = static_cast<StreamReader*>(opaque);
  // TODO: read
  //stream->source->Call()
  return 0;
}

int64_t StreamReader::Seek(void* opaque, int64_t offset, int whence) {
  StreamReader* stream = static_cast<StreamReader*>(opaque);
  // TODO: seek
  return 0;
}
