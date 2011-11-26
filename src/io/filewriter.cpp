#include "filewriter.h"

using namespace transcoding;
using namespace transcoding::io;

FileWriter::FileWriter(Handle<Object> source) :
    IOWriter(source) {
  HandleScope scope;

  this->path = *String::AsciiValue(source);
}

FileWriter::~FileWriter() {
}

int FileWriter::Open() {
  AVIOContext* s = NULL;
  int ret = avio_open(&s, this->path.c_str(), AVIO_WRONLY);
  if (ret) {
    return ret;
  }
  this->context = s;
  return 0;
}
