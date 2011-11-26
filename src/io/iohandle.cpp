#include "iohandle.h"
#include "filereader.h"
#include "filewriter.h"
#include "streamreader.h"

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

IOReader::IOReader(Handle<Object> source) :
    IOHandle(source) {
}

IOReader::~IOReader() {
}

IOReader* IOReader::Create(Handle<Object> source) {
  HandleScope scope;

  if (source->IsStringObject()) {
    return new FileReader(source);
  } else {
    return new StreamReader(source);
  }
}

IOWriter::IOWriter(Handle<Object> source) :
    IOHandle(source) {
}

IOWriter::~IOWriter() {
}

IOWriter* IOWriter::Create(Handle<Object> source) {
  HandleScope scope;

  if (source->IsStringObject()) {
    return new FileWriter(source);
  } else {
    //return new StreamWriter(source);
    return NULL;
  }
}
