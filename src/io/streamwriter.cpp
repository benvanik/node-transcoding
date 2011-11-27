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

typedef struct WriterAsyncRequest_t {
  uv_async_t      req;
  StreamWriter*   stream;
  WriteBuffer*    buffer;
} WriterAsyncRequest;

StreamWriter::StreamWriter(Handle<Object> source, size_t maxBufferedBytes) :
    IOWriter(source),
    paused(false), err(0), eof(false), closing(false),
    maxBufferedBytes(maxBufferedBytes), totalBufferredBytes(0),
    pendingWrites(0) {
  HandleScope scope;

  pthread_mutex_init(&this->lock, NULL);
  pthread_cond_init(&this->cond, NULL);

  // TODO: support seeking?
  this->canSeek = false;

  // Add events to stream
  // TODO: keep self alive somehow?
  NODE_ON_EVENT(source, "drain", OnDrain, this);
  NODE_ON_EVENT(source, "close", OnClose, this);
  NODE_ON_EVENT(source, "error", OnError, this);
}

StreamWriter::~StreamWriter() {
  printf("~StreamWriter\n");
  if (this->pendingWrites) {
    printf("WARNING: StreamWriter dtor when writes pending\n");
  }
  pthread_cond_destroy(&this->cond);
  pthread_mutex_destroy(&this->lock);
}

int StreamWriter::Open() {
  int bufferSize = STREAMWRITER_BUFFER_SIZE;
  uint8_t* buffer = (uint8_t*)av_malloc(bufferSize);
  AVIOContext* s = avio_alloc_context(
      buffer, bufferSize,
      1, // 1 = write
      this,
      NULL, WritePacket, this->canSeek ? Seek : NULL);
  s->seekable = 0; // AVIO_SEEKABLE_NORMAL
  this->context = s;
  return 0;
}

void StreamWriter::Close() {
  HandleScope scope;
  Local<Object> global = Context::GetCurrent()->Global();
  Handle<Object> source = this->source;

  // Unbind all events
  // NOTE: this will remove any user ones too, which could be bad...
  Local<Function> removeAllListeners =
      Local<Function>::Cast(source->Get(String::New("removeAllListeners")));
  removeAllListeners->Call(source,
      1, (Handle<Value>[]){ String::New("drain") });
  removeAllListeners->Call(source,
      1, (Handle<Value>[]){ String::New("close") });
  removeAllListeners->Call(source,
      1, (Handle<Value>[]){ String::New("error") });

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
}

bool StreamWriter::QueueCloseOnIdle() {
  pthread_mutex_lock(&this->lock);
  int pendingWrites = this->pendingWrites;
  this->closing = true;
  printf("pending writes: %d\n", pendingWrites);
  pthread_mutex_unlock(&this->lock);
  if (!pendingWrites) {
    // Close immediately
    printf("closing immediately\n");
    this->Close();
  }
  return pendingWrites > 0;
}

Handle<Value> StreamWriter::OnDrain(const Arguments& args) {
  HandleScope scope;
  StreamWriter* stream =
      static_cast<StreamWriter*>(External::Unwrap(args.Data()));

  pthread_mutex_lock(&stream->lock);
  stream->paused = false;
  pthread_cond_signal(&stream->cond);
  pthread_mutex_unlock(&stream->lock);

  return scope.Close(Undefined());
}

Handle<Value> StreamWriter::OnClose(const Arguments& args) {
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

  printf("StreamWriter::OnError %s\n", *String::Utf8Value(args[0]->ToString()));

  pthread_mutex_lock(&stream->lock);
  stream->err = AVERROR_IO;
  pthread_cond_signal(&stream->cond);
  pthread_mutex_unlock(&stream->lock);

  return scope.Close(Undefined());
}

void StreamWriter::WriteAsync(uv_async_t* handle, int status) {
  HandleScope scope;
  assert(status == 0);
  WriterAsyncRequest* req = static_cast<WriterAsyncRequest*>(handle->data);
  StreamWriter* stream = req->stream;
  Local<Object> global = Context::GetCurrent()->Global();

  // TODO: no-copy buffer (can just steal the data from the WriteBuffer)
  // TODO: fast buffer
  // NOTE: this may not be needed (according to node headers)
  // http://sambro.is-super-awesome.com/2011/03/03/creating-a-proper-buffer-in-a-node-c-addon/
  int bufferLength = (int)req->buffer->length;
  Buffer* slowBuffer = Buffer::New((char*)req->buffer->data, bufferLength);
  delete req->buffer;
  req->buffer = NULL;

  Handle<Value> ctorArgs[3] = {
      slowBuffer->handle_,
      Integer::New(bufferLength),
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
  bool bufferDrained =
      write->Call(stream->source, countof(argv), argv)->IsTrue();

  pthread_mutex_lock(&stream->lock);
  if (!bufferDrained) {
    // Buffer now full - wait until drain
    stream->paused = true;
  }
  stream->totalBufferredBytes -= bufferLength;
  stream->pendingWrites--;
  bool needsDestruction = stream->pendingWrites == 0 && stream->closing;
  pthread_mutex_unlock(&stream->lock);

  if (needsDestruction) {
    printf("closing at end of writes: %d\n", stream->pendingWrites);
    stream->Close();
    delete stream;
  }

  NODE_ASYNC_CLOSE(handle, AsyncHandleClose);
}

void StreamWriter::AsyncHandleClose(uv_handle_t* handle) {
  WriterAsyncRequest* req = static_cast<WriterAsyncRequest*>(handle->data);
  delete req;
  handle->data = NULL;
}

int StreamWriter::WritePacket(void* opaque, uint8_t* buffer, int bufferSize) {
  StreamWriter* stream = static_cast<StreamWriter*>(opaque);

  int ret = 0;
  bool needsResume = false;

  pthread_mutex_lock(&stream->lock);

  // Wait until the target is drained
  while (!stream->err && !stream->eof && stream->paused &&
      (stream->totalBufferredBytes >= stream->maxBufferedBytes)) {
    pthread_cond_wait(&stream->cond, &stream->lock);
  }

  if (stream->err) {
    // Stream error
    ret = stream->err;
  } else if (stream->totalBufferredBytes < stream->maxBufferedBytes) {
    // Write the next buffer
    WriteBuffer* nextBuffer = new WriteBuffer((uint8_t*)buffer, bufferSize);
    stream->totalBufferredBytes += nextBuffer->length;
    stream->pendingWrites++;

    WriterAsyncRequest* asyncReq = new WriterAsyncRequest();
    asyncReq->req.data = asyncReq;
    asyncReq->stream = stream;
    asyncReq->buffer = nextBuffer;
    uv_async_init(uv_default_loop(), &asyncReq->req, WriteAsync);
    uv_async_send(&asyncReq->req);

    ret = bufferSize;
  } else if (stream->eof) {
    // Stream at EOF
    ret = 0; // eof
  } else {
    // Stream in error (or unknown, so return EOF)
    ret = stream->err;
  }

  if (stream->totalBufferredBytes >= stream->maxBufferedBytes) {
    stream->paused = true;
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
  delete[] this->data;
  this->data = NULL;
}
