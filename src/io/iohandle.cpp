#include "iohandle.h"
#include "filereader.h"
#include "filewriter.h"
#include "streamreader.h"
#include "streamwriter.h"

using namespace transcoding;
using namespace transcoding::io;

IOHandle::IOHandle(Handle<Object> source) :
    context(NULL) {
  HandleScope scope;

  this->source = Persistent<Object>::New(source);
}

IOHandle::~IOHandle() {
  this->source.Dispose();
}

void IOHandle::CloseWhenDone(IOHandle* handle) {
  if (handle->QueueCloseOnIdle()) {
    // Queued! Will be deleted at some point
  } else {
    // Not queued - delete now
    delete handle;
  }
}

bool IOHandle::QueueCloseOnIdle() {
  this->Close();
  return false;
}

IOReader::IOReader(Handle<Object> source) :
    IOHandle(source) {
}

IOReader::~IOReader() {
}

IOReader* IOReader::Create(Handle<Object> source, size_t maxBufferedBytes) {
  HandleScope scope;

  if (source->IsStringObject()) {
    return new FileReader(source);
  } else {
    return new StreamReader(source,
        maxBufferedBytes ? maxBufferedBytes : STREAMREADER_MAX_SIZE);
  }
}

IOWriter::IOWriter(Handle<Object> source) :
    IOHandle(source) {
}

IOWriter::~IOWriter() {
}

IOWriter* IOWriter::Create(Handle<Object> source, size_t maxBufferedBytes) {
  HandleScope scope;

  if (source->IsStringObject()) {
    return new FileWriter(source);
  } else {
    return new StreamWriter(source,
        maxBufferedBytes ? maxBufferedBytes : STREAMWRITER_MAX_SIZE);
  }
}
