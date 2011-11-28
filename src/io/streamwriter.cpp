#include "streamwriter.h"
#include <node_buffer.h>

using namespace node;
using namespace transcoding;
using namespace transcoding::io;

// The stream writer works by firing async write requests to the v8 thread to
// pass off to the stream object. When the stream object indicates that its
// buffers are full the WritePacket calls occuring on the background thread will
// block. Once the stream object has drained, signalling it's ok to write again,
// writes will continue.

StreamWriter::StreamWriter(Handle<Object> source, size_t maxBufferedBytes) :
    IOWriter(source),
    idle(NULL),
    kernelBufferFull(false), selfBufferFull(false),
    err(0), eof(false), closing(false),
    maxBufferedBytes(maxBufferedBytes), totalBufferredBytes(0) {
  TC_LOG_D("StreamWriter::StreamWriter()\n");
  HandleScope scope;

  pthread_mutex_init(&this->lock, NULL);
  pthread_cond_init(&this->cond, NULL);

  // TODO: support seeking?
  this->canSeek = false;

  // Add events to stream
  // TODO: keep self alive somehow?
  NODE_ON_EVENT(source, "drain", onDrain, OnDrain, this);
  NODE_ON_EVENT(source, "close", onClose, OnClose, this);
  NODE_ON_EVENT(source, "error", onError, OnError, this);
}

StreamWriter::~StreamWriter() {
  TC_LOG_D("StreamWriter::~StreamWriter()\n");

  pthread_mutex_lock(&this->lock);
  if (this->buffers.size()) {
    TC_LOG_D("StreamWriter::~StreamWriter(): dtor with %d writes pending\n",
        (int)this->buffers.size());
  }
  pthread_mutex_unlock(&this->lock);
  pthread_cond_destroy(&this->cond);
  pthread_mutex_destroy(&this->lock);

  // NOTE: do not delete this->idle, it is cleaned up by the handle close stuff
}

int StreamWriter::Open() {
  TC_LOG_D("StreamWriter::Open()\n");

  int bufferSize = STREAMWRITER_BUFFER_SIZE;
  uint8_t* buffer = (uint8_t*)av_malloc(bufferSize);
  AVIOContext* s = avio_alloc_context(
      buffer, bufferSize,
      1, // 1 = write
      this,
      NULL, WritePacket, this->canSeek ? Seek : NULL);
  s->seekable = 0; // AVIO_SEEKABLE_NORMAL
  this->context = s;

  this->idle = new uv_idle_t();
  this->idle->data = this;
  uv_idle_init(uv_default_loop(), this->idle);
  uv_idle_start(this->idle, IdleCallback);

  return 0;
}

void StreamWriter::Close() {
  TC_LOG_D("StreamWriter::Close()\n");
  HandleScope scope;
  Local<Object> source = Local<Object>::New(this->source);

  pthread_mutex_lock(&this->lock);
  if (this->buffers.size()) {
    TC_LOG_D("StreamWriter::Close(): close when %d writes pending\n",
        (int)this->buffers.size());
  }
  pthread_mutex_unlock(&this->lock);

  // Unbind all events
  NODE_REMOVE_EVENT(source, "drain", onDrain);
  NODE_REMOVE_EVENT(source, "close", onClose);
  NODE_REMOVE_EVENT(source, "error", onError);

  bool writable = source->Get(String::New("writable"))->IsTrue();
  if (writable) {
    Local<Function> end =
        Local<Function>::Cast(this->source->Get(String::New("end")));
    end->Call(this->source, 0, NULL);

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

  // Kill the idle ASAP
  uv_close((uv_handle_t*)this->idle, IdleHandleClose);
}

bool StreamWriter::QueueCloseOnIdle() {
  TC_LOG_D("StreamWriter::QueueCloseOnIdle()\n");

  pthread_mutex_lock(&this->lock);
  int pendingWrites = this->buffers.size();
  this->closing = true;
  pthread_cond_signal(&this->cond);
  pthread_mutex_unlock(&this->lock);
  if (!pendingWrites) {
    // Close immediately
    TC_LOG_D("StreamWriter::QueueCloseOnIdle(): closing immediately\n");
    this->Close();
  } else {
    TC_LOG_D("StreamWriter::QueueCloseOnIdle(): deferred close, %d pending\n",
        pendingWrites);
  }
  return pendingWrites > 0;
}

Handle<Value> StreamWriter::OnDrain(const Arguments& args) {
  TC_LOG_D("StreamWriter::OnDrain()\n");
  HandleScope scope;
  StreamWriter* stream =
      static_cast<StreamWriter*>(External::Unwrap(args.Data()));

  pthread_mutex_lock(&stream->lock);
  stream->kernelBufferFull = false;
  pthread_cond_signal(&stream->cond);
  pthread_mutex_unlock(&stream->lock);

  return scope.Close(Undefined());
}

Handle<Value> StreamWriter::OnClose(const Arguments& args) {
  TC_LOG_D("StreamWriter::OnClose()\n");
  HandleScope scope;
  StreamWriter* stream =
      static_cast<StreamWriter*>(External::Unwrap(args.Data()));

  pthread_mutex_lock(&stream->lock);
  stream->eof = true;
  pthread_cond_signal(&stream->cond);
  pthread_mutex_unlock(&stream->lock);

  return scope.Close(Undefined());
}

Handle<Value> StreamWriter::OnError(const Arguments& args) {
  HandleScope scope;
  StreamWriter* stream =
      static_cast<StreamWriter*>(External::Unwrap(args.Data()));

  TC_LOG_D("StreamWriter::OnError(): %s\n",
      *String::Utf8Value(args[0]->ToString()));

  pthread_mutex_lock(&stream->lock);
  stream->err = AVERROR_IO;
  pthread_cond_signal(&stream->cond);
  pthread_mutex_unlock(&stream->lock);

  return scope.Close(Undefined());
}

void StreamWriterFreeCallback(char *data, void *hint) {
  uint8_t* ptr = (uint8_t*)data;
  delete[] ptr;
}

void StreamWriter::IdleCallback(uv_idle_t* handle, int status) {
  HandleScope scope;
  assert(status == 0);
  StreamWriter* stream = static_cast<StreamWriter*>(handle->data);

  if (stream->kernelBufferFull) {
    // Don't do anything this tick
    return;
  }

  bool needsDestruction = false;

  while (true) {
    WriteBuffer* buffer = NULL;
    pthread_mutex_lock(&stream->lock);
    int remaining = stream->buffers.size();
    if (remaining) {
      buffer = stream->buffers.front();
      stream->buffers.erase(stream->buffers.begin());
    } else {
      needsDestruction = stream->closing;
    }
    pthread_mutex_unlock(&stream->lock);
    if (!buffer) {
      break;
    }

    TC_LOG_D("StreamWriter::IdleCallback(): write, %d remaining, closing: %d\n",
        remaining, stream->closing);

    // TODO: coalesce buffers (if 100 buffers waiting, merge?)
    // Wrap our buffer in the node buffer (pass reference)
    int bufferLength = (int)buffer->length;
    Buffer* bufferObj = Buffer::New(
        (char*)buffer->data, bufferLength, StreamWriterFreeCallback, NULL);
    Local<Object> bufferLocal = Local<Object>::New(bufferObj->handle_);
    buffer->Steal();
    delete buffer;

    Local<Function> write =
        Local<Function>::Cast(stream->source->Get(String::New("write")));
    bool bufferDrained =
        write->Call(stream->source, 1, (Handle<Value>[]){
          bufferLocal,
        })->IsTrue();

    pthread_mutex_lock(&stream->lock);
    if (!stream->kernelBufferFull && !bufferDrained) {
      // Buffer now full - wait until drain
      stream->kernelBufferFull = true;
      TC_LOG_D("StreamWriter::IdleCallback(): kernel buffer full\n");
    }
    stream->totalBufferredBytes -= bufferLength;

    // Drain until the buffer has a bit of room
    bool bufferFull =
        stream->totalBufferredBytes + STREAMWRITER_BUFFER_SIZE * 4 >=
        stream->maxBufferedBytes;
    if (stream->selfBufferFull && !bufferFull) {
      stream->selfBufferFull = false;
      TC_LOG_D("StreamWriter::IdleCallback(): self buffer drained\n");
    }

    pthread_cond_signal(&stream->cond);
    pthread_mutex_unlock(&stream->lock);
    if (!bufferDrained) {
      // Don't consume any more buffers
      break;
    }

    // Can get in very bad tight loops with this logic - only write until there
    // is room enough in the buffer for more writes
    // Always drain when closing, though!
    // TODO: when closing, do a nice staggered write sequence through the main
    // event loop - if there are many pending writes this can block for awhile!
    if (bufferFull) {
      TC_LOG_D("StreamWriter::IdleCallback(): self buffer drained, break\n");
      break;
    }
  }

  if (needsDestruction) {
    TC_LOG_D("StreamWriter::IdleCallback(): closing at end of writes\n");
    stream->Close();
    delete stream;
  }
}

void StreamWriter::IdleHandleClose(uv_handle_t* handle) {
  TC_LOG_D("StreamWriter::IdleHandleClose()\n");
  delete handle;
}

int StreamWriter::WritePacket(void* opaque, uint8_t* buffer, int bufferSize) {
  TC_LOG_D("StreamWriter::WritePacket(%d)\n", bufferSize);
  StreamWriter* stream = static_cast<StreamWriter*>(opaque);

  int ret = 0;
  bool needsResume = false;

  pthread_mutex_lock(&stream->lock);

  if (!stream->selfBufferFull &&
      stream->totalBufferredBytes + bufferSize > stream->maxBufferedBytes) {
    TC_LOG_D("StreamWriter::WritePacket(): self buffer full\n");
    stream->selfBufferFull = true;
  }

  // Wait until the target is drained
  while (!stream->err && !stream->eof &&
      (stream->kernelBufferFull || stream->selfBufferFull)) {
    pthread_cond_wait(&stream->cond, &stream->lock);
  }

  TC_LOG_D("StreamWriter::WritePacket(): %lld/%lld, eof %d, err %d, kbf %d, sbf %d\n",
      stream->totalBufferredBytes, stream->maxBufferedBytes,
      stream->eof, stream->err,
      stream->kernelBufferFull, stream->selfBufferFull);

  if (stream->err) {
    // Stream error
    ret = stream->err;
    TC_LOG_D("StreamWriter::WritePacket(): stream error (%d)\n", stream->err);
  } else if (stream->totalBufferredBytes < stream->maxBufferedBytes) {
    // Write the next buffer
    WriteBuffer* nextBuffer = new WriteBuffer((uint8_t*)buffer, bufferSize);
    stream->buffers.push_back(nextBuffer);
    stream->totalBufferredBytes += nextBuffer->length;

    TC_LOG_D("StreamWriter::WritePacket(): write of %d bytes\n", bufferSize);

    ret = bufferSize;
  } else if (stream->eof) {
    // Stream at EOF
    TC_LOG_D("StreamWriter::WritePacket(): stream eof\n");
    ret = 0; // eof
  } else {
    // Stream in error (or unknown, so return EOF)
    TC_LOG_D("StreamWriter::WritePacket(): stream UNKNOWN (%d)\n", stream->err);
    ret = stream->err;
  }

  pthread_mutex_unlock(&stream->lock);

  return ret;
}

int64_t StreamWriter::Seek(void* opaque, int64_t offset, int whence) {
  StreamWriter* stream = static_cast<StreamWriter*>(opaque);
  // TODO: seek
  return 0;
}

WriteBuffer::WriteBuffer(uint8_t* source, int64_t length) :
    data(NULL), length(length) {
  this->length = length;
  this->data = new uint8_t[this->length];
  memcpy(this->data, source, this->length);
}

WriteBuffer::~WriteBuffer() {
  if (this->data) {
    delete[] this->data;
    this->data = NULL;
  }
}

void WriteBuffer::Steal() {
  this->data = NULL;
  this->length = 0;
}
