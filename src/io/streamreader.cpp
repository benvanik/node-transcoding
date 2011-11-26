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
  HandleScope scope;

  pthread_mutex_init(&this->lock, NULL);
  pthread_cond_init(&this->cond, NULL);

  // TODO: support seeking?
  this->canSeek = false;

  // Add events to stream
  // TODO: keep self alive somehow?
  NODE_ON_EVENT(source, "data", OnData, this);
  NODE_ON_EVENT(source, "end", OnEnd, this);
  NODE_ON_EVENT(source, "close", OnClose, this);
  NODE_ON_EVENT(source, "error", OnError, this);

  // Kick off the stream (some sources need this)
  Local<Function> resume =
      Local<Function>::Cast(source->Get(String::New("resume")));
  if (!resume.IsEmpty()) {
    resume->Call(source, 0, NULL);
  }
}

StreamReader::~StreamReader() {
  pthread_cond_destroy(&this->cond);
  pthread_mutex_destroy(&this->lock);
}

int StreamReader::Open() {
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
  HandleScope scope;
  Local<Object> global = Context::GetCurrent()->Global();
  Handle<Object> source = this->source;

  // Unbind all events
  // NOTE: this will remove any user ones too, which could be bad...
  Local<Function> removeAllListeners =
      Local<Function>::Cast(source->Get(String::New("removeAllListeners")));
  removeAllListeners->Call(source,
      1, (Handle<Value>[]){ String::New("data") });
  removeAllListeners->Call(source,
      1, (Handle<Value>[]){ String::New("end") });
  removeAllListeners->Call(source,
      1, (Handle<Value>[]){ String::New("close") });
  removeAllListeners->Call(source,
      1, (Handle<Value>[]){ String::New("error") });

  bool readable = source->Get(String::New("readable"))->IsTrue();
  if (readable) {
    Local<Function> destroy =
        Local<Function>::Cast(source->Get(String::New("destroy")));
    if (!destroy.IsEmpty()) {
      destroy->Call(source, 0, NULL);
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

  pthread_mutex_lock(&stream->lock);

  Local<Object> buffer = Local<Object>::Cast(args[0]);
  ReadBuffer* readBuffer = new ReadBuffer(
      (uint8_t*)Buffer::Data(buffer), Buffer::Length(buffer));
  stream->buffers.push_back(readBuffer);
  stream->totalBufferredBytes += readBuffer->length;

  // Check for max buffer condition
  bool needsPause = false;
  if (stream->totalBufferredBytes > stream->maxBufferedBytes) {
    //needsPause = true;
    stream->paused = true;
  }

  //pthread_cond_signal(&stream->cond);
  pthread_mutex_unlock(&stream->lock);

  if (needsPause) {
    Local<Function> pause =
        Local<Function>::Cast(stream->source->Get(String::New("pause")));
    if (!pause.IsEmpty()) {
      pause->Call(stream->source, 0, NULL);
    }
  }

  return scope.Close(Undefined());
}

Handle<Value> StreamReader::OnEnd(const Arguments& args) {
  HandleScope scope;
  StreamReader* stream =
      static_cast<StreamReader*>(External::Unwrap(args.Data()));

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

  printf("StreamReader::OnError %s\n", *String::Utf8Value(args[0]->ToString()));

  pthread_mutex_lock(&stream->lock);
  stream->err = AVERROR_IO;
  pthread_cond_signal(&stream->cond);
  pthread_mutex_unlock(&stream->lock);

  return scope.Close(Undefined());
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
    ret = stream->err;
  } else if (stream->totalBufferredBytes) {
    // Read the next buffer
    ReadBuffer* nextBuffer = stream->buffers.front();
    size_t bytesRead = nextBuffer->Read(buffer, bufferSize);
    if (nextBuffer->IsEmpty()) {
      stream->totalBufferredBytes -= nextBuffer->length;
      stream->buffers.erase(stream->buffers.begin());
      delete nextBuffer;
    }
    ret = (int)bytesRead;
  } else if (stream->eof) {
    // Stream at EOF
    ret = 0; // eof
  } else {
    // Stream in error (or unknown, so return EOF)
    ret = stream->err;
  }

  if (stream->paused && ret > 0) {
    // Stream is paused - restart it
    if (stream->totalBufferredBytes < stream->maxBufferedBytes) {
      needsResume = true;
    }
  }

  pthread_mutex_unlock(&stream->lock);

  if (needsResume) {
    // TODO: issue async resume
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
