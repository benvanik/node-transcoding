#include "filereader.h"

using namespace transcoding;
using namespace transcoding::io;

FileReader::FileReader(Handle<Object> source) :
    IOReader(source) {
  HandleScope scope;
  TC_LOG_D("FileReader::FileReader(%s)\n", *String::Utf8Value(source));

  this->path = *String::AsciiValue(source);
}

FileReader::~FileReader() {
  TC_LOG_D("FileReader::~FileReader()\n");
}

int FileReader::Open() {
  AVIOContext* s = NULL;
  int ret = avio_open(&s, this->path.c_str(), AVIO_RDONLY);
  if (ret) {
    TC_LOG_D("FileReader::Open(): failed (%d)\n", ret);
    return ret;
  }
  TC_LOG_D("FileReader::Open()\n");
  this->context = s;
  return 0;
}

void FileReader::Close() {
  TC_LOG_D("FileReader::Close()\n");
  avio_close(this->context);
  this->context = NULL;
}
