#include "io.h"
#include <node_buffer.h>

using namespace node;
using namespace transcoding;

IOHandle::IOHandle(Handle<Object> source) {
  HandleScope scope;

  this->source = Persistent<Object>::New(source);
}

IOHandle::~IOHandle() {
  this->source.Dispose();
}

IOHandle* IOHandle::Create(Handle<Object> source) {
  HandleScope scope;

  if (source->IsStringObject()) {
    return new FileHandle(source);
  } else {
    return new StreamHandle(source);
  }
}

FileHandle::FileHandle(Handle<Object> source) :
    IOHandle(source) {
  HandleScope scope;

  this->path = *String::AsciiValue(source);
}

FileHandle::~FileHandle() {
}

AVIOContext* FileHandle::OpenRead() {
  AVIOContext* s = NULL;
  int ret = avio_open(&s, this->path.c_str(), AVIO_RDONLY);
  if (ret) {
    return NULL;
  }
  return s;
}

AVIOContext* FileHandle::OpenWrite() {
  AVIOContext* s = NULL;
  int ret = avio_open(&s, this->path.c_str(), AVIO_WRONLY);
  if (ret) {
    return NULL;
  }
  return s;
}

void FileHandle::Close(AVIOContext* s) {
  avio_close(s);
}

StreamHandle::StreamHandle(Handle<Object> source) :
    IOHandle(source) {
  HandleScope scope;

  // TODO: detect if can seek
  this->canSeek = false;
}

StreamHandle::~StreamHandle() {
}

#define STREAM_HANDLE_BUFFER_SIZE (64 * 1024)

AVIOContext* StreamHandle::OpenRead() {
  int bufferSize = STREAM_HANDLE_BUFFER_SIZE;
  uint8_t* buffer = (uint8_t*)av_malloc(bufferSize);
  AVIOContext* s = avio_alloc_context(
      buffer, bufferSize,
      0, // 1 = write
      this,
      ReadPacket, NULL, this->canSeek ? Seek : NULL);
  return s;
}

AVIOContext* StreamHandle::OpenWrite() {
  int bufferSize = STREAM_HANDLE_BUFFER_SIZE;
  uint8_t* buffer = (uint8_t*)av_malloc(bufferSize);
  AVIOContext* s = avio_alloc_context(
      buffer, bufferSize,
      1, // 1 = write
      this,
      NULL, WritePacket, this->canSeek ? Seek : NULL);
  return s;
}

void StreamHandle::Close(AVIOContext* s) {
  HandleScope scope;
  Local<Object> global = Context::GetCurrent()->Global();

  Local<Function> end =
      Local<Function>::Cast(this->source->Get(String::New("end")));
  end->Call(this->source, 0, NULL);

  av_free(s);
}

int StreamHandle::ReadPacket(void* opaque, uint8_t* buffer, int bufferSize) {
  HandleScope scope;
  StreamHandle* stream = static_cast<StreamHandle*>(opaque);
  // TODO: read
  //stream->source->Call()
  return 0;
}

int StreamHandle::WritePacket(void* opaque, uint8_t* buffer, int bufferSize) {
  HandleScope scope;
  StreamHandle* stream = static_cast<StreamHandle*>(opaque);
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

int64_t StreamHandle::Seek(void* opaque, int64_t offset, int whence) {
  HandleScope scope;
  StreamHandle* stream = static_cast<StreamHandle*>(opaque);
  // TODO: seek
  //stream->source->Call()
  return 0;
}

LiveStreamingHandle::LiveStreamingHandle(Handle<Object> source) :
    IOHandle(source) {
  HandleScope scope;

  this->path = *String::AsciiValue(source);
}

LiveStreamingHandle::~LiveStreamingHandle() {
}

AVFormatContext* transcoding::createInputContext(IOHandle* input, int* pret) {
  AVFormatContext* ctx = NULL;
  int ret = 0;
  *pret = 0;

  ctx = avformat_alloc_context();
  if (!ctx) {
    ret = AVERROR_NOMEM;
    goto CLEANUP;
  }

  ctx->pb = input->OpenRead();
  if (!ctx->pb) {
    ret = AVERROR_NOENT;
    goto CLEANUP;
  }

  ret = avformat_open_input(&ctx, "", NULL, NULL);
  if (ret < 0) {
    goto CLEANUP;
  }

  ret = av_find_stream_info(ctx);
  if (ret < 0) {
    goto CLEANUP;
  }

  return ctx;

CLEANUP:
  if (ctx) {
    avformat_free_context(ctx);
  }
  *pret = ret;
  return NULL;
}

AVFormatContext* transcoding::createOutputContext(IOHandle* output, int* pret) {
  AVFormatContext* ctx = NULL;
  int ret = 0;
  *pret = 0;

  ctx = avformat_alloc_context();
  if (!ctx) {
    ret = AVERROR_NOMEM;
    goto CLEANUP;
  }

  ctx->pb = output->OpenWrite();
  if (!ctx->pb) {
    ret = AVERROR_NOENT;
    goto CLEANUP;
  }

  return ctx;

CLEANUP:
  if (ctx) {
    avformat_free_context(ctx);
  }
  *pret = ret;
  return NULL;
}
