#include "streamreader.h"
#include <node_buffer.h>

using namespace node;
using namespace transcoding;
using namespace transcoding::io;

// The stream reader works by listening for data events on the v8 thread
// and queuing them up. The actual ReadPacket calls come across on a worker
// thread and pop them off. If there are no buffers available, the worker thread
// will block until there are. If there are too many buffers queued, the stream
// is paused until buffers are drained.

StreamReader::StreamReader(Handle<Object> source, size_t maxBufferedBytes) :
    IOReader(source),
    paused(false), err(0), eof(false),
    maxBufferedBytes(maxBufferedBytes), totalBufferredBytes(0) {
  TC_LOG_D("StreamReader::StreamReader()\n");
  HandleScope scope;

  pthread_mutex_init(&this->lock, NULL);
  pthread_cond_init(&this->cond, NULL);

  this->asyncReq = new uv_async_t();
  this->asyncReq->data = this;
  uv_async_init(uv_default_loop(), this->asyncReq, ResumeAsync);

  // TODO: support seeking?
  this->canSeek = false;

  // Pull out methods we will use frequently
  this->sourcePause = Persistent<Function>::New(
      this->source->Get(String::New("pause")).As<Function>());
  this->sourceResume = Persistent<Function>::New(
      this->source->Get(String::New("resume")).As<Function>());

  // Add events to stream
  NODE_ON_EVENT(source, "data", onData, OnData, this);
  NODE_ON_EVENT(source, "end", onEnd, OnEnd, this);
  NODE_ON_EVENT(source, "close", onClose, OnClose, this);
  NODE_ON_EVENT(source, "error", onError, OnError, this);

  // Kick off the stream (some sources need this)
  if (!this->sourceResume.IsEmpty()) {
    this->sourceResume->Call(this->source, 0, NULL);
  }
}

StreamReader::~StreamReader() {
  HandleScope scope;

  TC_LOG_D("StreamReader::~StreamReader()\n");
  pthread_cond_destroy(&this->cond);
  pthread_mutex_destroy(&this->lock);

  this->sourcePause.Dispose();
  this->sourceResume.Dispose();

  uv_close((uv_handle_t*)this->asyncReq, AsyncHandleClose);
}

int StreamReader::Open() {
  TC_LOG_D("StreamReader::Open()\n");
  int bufferSize = STREAMREADER_BUFFER_SIZE;
  uint8_t* buffer = (uint8_t*)av_malloc(bufferSize);
  AVIOContext* s = avio_alloc_context(
      buffer, bufferSize,
      0, // 1 = write
      this,
      ReadPacket, NULL, this->canSeek ? Seek : NULL);
  s->seekable = 0; // AVIO_SEEKABLE_NORMAL
  this->context = s;
  return 0;
}

void StreamReader::Close() {
  TC_LOG_D("StreamReader::Close()\n");
  HandleScope scope;
  Handle<Object> source = this->source;

  // Unbind all events
  NODE_REMOVE_EVENT(source, "data", onData);
  NODE_REMOVE_EVENT(source, "end", onEnd);
  NODE_REMOVE_EVENT(source, "close", onClose);
  NODE_REMOVE_EVENT(source, "error", onError);

  bool readable = source->Get(String::New("readable"))->IsTrue();
  if (readable) {
    Local<Function> destroySoon =
        Local<Function>::Cast(source->Get(String::New("destroySoon")));
    if (!destroySoon.IsEmpty()) {
      destroySoon->Call(source, 0, NULL);
    }
  }

  if (this->context->buffer) {
    av_free(this->context->buffer);
  }
  av_free(this->context);
  this->context = NULL;
}

Handle<Value> StreamReader::OnData(const Arguments& args) {
  HandleScope scope;
  StreamReader* stream =
      static_cast<StreamReader*>(External::Unwrap(args.Data()));

  Local<Object> buffer = Local<Object>::Cast(args[0]);
  ReadBuffer* readBuffer = new ReadBuffer(
      (uint8_t*)Buffer::Data(buffer), Buffer::Length(buffer));

  TC_LOG_D("StreamReader::OnData(): %d new bytes\n",
      (int)Buffer::Length(buffer));

  pthread_mutex_lock(&stream->lock);

  stream->buffers.push_back(readBuffer);
  stream->totalBufferredBytes += readBuffer->length;

  // Check for max buffer condition
  bool needsPause = false;
  if (stream->totalBufferredBytes > stream->maxBufferedBytes) {
    if (!stream->sourcePause.IsEmpty()) {
      needsPause = true;
      stream->paused = true;
    }
  }

  //printf("OnData: buffer %lld/%lld, paused: %d\n",
  //    stream->totalBufferredBytes, stream->maxBufferedBytes, stream->paused);

  pthread_cond_signal(&stream->cond);
  pthread_mutex_unlock(&stream->lock);

  if (needsPause) {
    TC_LOG_D("StreamReader::OnData(): buffer full, pausing\n");
    stream->sourcePause->Call(stream->source, 0, NULL);
  }

  return scope.Close(Undefined());
}

Handle<Value> StreamReader::OnEnd(const Arguments& args) {
  HandleScope scope;
  StreamReader* stream =
      static_cast<StreamReader*>(External::Unwrap(args.Data()));

  TC_LOG_D("StreamReader::OnEnd()\n");

  pthread_mutex_lock(&stream->lock);
  stream->eof = true;
  pthread_cond_signal(&stream->cond);
  pthread_mutex_unlock(&stream->lock);

  return scope.Close(Undefined());
}

Handle<Value> StreamReader::OnClose(const Arguments& args) {
  HandleScope scope;
  StreamReader* stream =
      static_cast<StreamReader*>(External::Unwrap(args.Data()));

  TC_LOG_D("StreamReader::OnClose()\n");

  pthread_mutex_lock(&stream->lock);
  stream->eof = true;
  pthread_cond_signal(&stream->cond);
  pthread_mutex_unlock(&stream->lock);

  return scope.Close(Undefined());
}

Handle<Value> StreamReader::OnError(const Arguments& args) {
  HandleScope scope;
  StreamReader* stream =
      static_cast<StreamReader*>(External::Unwrap(args.Data()));

  TC_LOG_D("StreamReader::OnError(): %s\n",
      *String::Utf8Value(args[0]->ToString()));

  pthread_mutex_lock(&stream->lock);
  stream->err = AVERROR_IO;
  pthread_cond_signal(&stream->cond);
  pthread_mutex_unlock(&stream->lock);

  return scope.Close(Undefined());
}

void StreamReader::ResumeAsync(uv_async_t* handle, int status) {
  HandleScope scope;
  assert(status == 0);
  StreamReader* stream = static_cast<StreamReader*>(handle->data);

  if (!stream->sourceResume.IsEmpty()) {
    TC_LOG_D("StreamReader::ResumeAsync()\n");
    stream->sourceResume->Call(stream->source, 0, NULL);
  }
}

void StreamReader::AsyncHandleClose(uv_handle_t* handle) {
  TC_LOG_D("StreamReader::AsyncHandleClose()\n");
  delete handle;
}

int StreamReader::ReadPacket(void* opaque, uint8_t* buffer, int bufferSize) {
  StreamReader* stream = static_cast<StreamReader*>(opaque);

  int ret = 0;
  bool needsResume = false;

  pthread_mutex_lock(&stream->lock);

  // Wait until some bytes are available
  while (!stream->err && !stream->eof && !stream->totalBufferredBytes) {
    pthread_cond_wait(&stream->cond, &stream->lock);
  }

  if (stream->err) {
    // Stream error
    TC_LOG_D("StreamReader::ReadPacket(): stream error (%d)\n", stream->err);
    ret = stream->err;
  } else if (stream->totalBufferredBytes) {
    // Read the next buffer
    ReadBuffer* nextBuffer = stream->buffers.front();
    size_t bytesRead = nextBuffer->Read(buffer, bufferSize);
    stream->totalBufferredBytes -= bytesRead;
    assert(stream->totalBufferredBytes >= 0);
    if (nextBuffer->IsEmpty()) {
      stream->buffers.erase(stream->buffers.begin());
      delete nextBuffer;
    }
    TC_LOG_D("StreamReader::ReadPacket(): reading %d bytes\n", (int)bytesRead);
    ret = (int)bytesRead;
  } else if (stream->eof) {
    // Stream at EOF
    TC_LOG_D("StreamReader::ReadPacket(): stream eof\n");
    ret = 0; // eof
  } else {
    // Stream in error (or unknown, so return EOF)
    TC_LOG_D("StreamReader::ReadPacket(): stream UNKNOWN (%d)\n", stream->err);
    ret = stream->err;
  }

  if (stream->paused && ret > 0) {
    // Stream is paused - restart it
    if (stream->totalBufferredBytes < stream->maxBufferedBytes) {
      needsResume = true;
      stream->paused = false;
    }
  }

  pthread_mutex_unlock(&stream->lock);

  //printf("ReadPacket: buffer %lld/%lld, paused: %d, resuming: %d\n",
  //    stream->totalBufferredBytes, stream->maxBufferedBytes, stream->paused,
  //    needsResume);

  if (needsResume) {
    TC_LOG_D("StreamReader::ReadPacket(): resuming stream\n");
    uv_async_send(stream->asyncReq);
  }

  return ret;
}

int64_t StreamReader::Seek(void* opaque, int64_t offset, int whence) {
  StreamReader* stream = static_cast<StreamReader*>(opaque);
  // TODO: seek
  return 0;
}

ReadBuffer::ReadBuffer(uint8_t* source, int64_t length) :
    offset(0), data(NULL), length(length) {
  this->length = length;
  this->data = new uint8_t[this->length];
  memcpy(this->data, source, this->length);
}

ReadBuffer::~ReadBuffer() {
  delete[] this->data;
  this->data = NULL;
}

bool ReadBuffer::IsEmpty() {
  return this->offset >= this->length || !this->length;
}

int64_t ReadBuffer::Read(uint8_t* buffer, int64_t bufferSize) {
  int64_t toRead = std::min(bufferSize, this->length - this->offset);
  if (toRead) {
    memcpy(buffer, this->data + this->offset, toRead);
    this->offset += toRead;
  }
  return toRead;
}
