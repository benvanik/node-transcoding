#include "filereader.h"

using namespace transcoding;
using namespace transcoding::io;

FileReader::FileReader(Handle<Object> source) :
    IOReader(source) {
  HandleScope scope;

  this->path = *String::AsciiValue(source);
}

FileReader::~FileReader() {
}

int FileReader::Open() {
  AVIOContext* s = NULL;
  int ret = avio_open(&s, this->path.c_str(), AVIO_RDONLY);
  if (ret) {
    return ret;
  }
  this->context = s;
  return 0;
}

void FileReader::Close() {
  avio_close(this->context);
  this->context = NULL;
}
