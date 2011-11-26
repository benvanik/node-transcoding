#include "streamwriter.h"
#include <node_buffer.h>

using namespace node;
using namespace transcoding;
using namespace transcoding::io;

#define STREAM_HANDLE_BUFFER_SIZE (64 * 1024)

StreamWriter::StreamWriter(Handle<Object> source) :
    IOWriter(source) {
  HandleScope scope;

  // TODO: support seeking?
  this->canSeek = false;
}

StreamWriter::~StreamWriter() {
}

int StreamWriter::Open() {
  int bufferSize = STREAM_HANDLE_BUFFER_SIZE;
  uint8_t* buffer = (uint8_t*)av_malloc(bufferSize);
  AVIOContext* s = avio_alloc_context(
      buffer, bufferSize,
      1, // 1 = write
      this,
      NULL, WritePacket, this->canSeek ? Seek : NULL);
  this->context = s;
  return 0;
}

void StreamWriter::Close() {
  HandleScope scope;
  Local<Object> global = Context::GetCurrent()->Global();

  Local<Function> end =
      Local<Function>::Cast(this->source->Get(String::New("end")));
  end->Call(this->source, 0, NULL);

  av_free(this->context);
  this->context = NULL;
}

// TODO: this won't work, as it's on the background thread - need to marshal
int StreamWriter::WritePacket(void* opaque, uint8_t* buffer, int bufferSize) {
  HandleScope scope;
  StreamWriter* stream = static_cast<StreamWriter*>(opaque);
  Local<Object> global = Context::GetCurrent()->Global();

  // TODO: fast buffer
  // http://sambro.is-super-awesome.com/2011/03/03/creating-a-proper-buffer-in-a-node-c-addon/
  Buffer* slowBuffer = Buffer::New((char*)buffer, bufferSize);
  Handle<Value> ctorArgs[3] = {
      slowBuffer->handle_,
      Integer::New(bufferSize),
      Integer::New(0),
  };
  Local<Function> bufferCtor =
      Local<Function>::Cast(global->Get(String::New("Buffer")));
  Local<Object> actualBuffer =
      bufferCtor->NewInstance(countof(ctorArgs), ctorArgs);

  Handle<Value> argv[] = {
    actualBuffer,
  };

  Local<Function> write =
      Local<Function>::Cast(stream->source->Get(String::New("write")));
  write->Call(stream->source, countof(argv), argv);

  return bufferSize;
}

int64_t StreamWriter::Seek(void* opaque, int64_t offset, int whence) {
  StreamWriter* stream = static_cast<StreamWriter*>(opaque);
  // TODO: seek
  return 0;
}
