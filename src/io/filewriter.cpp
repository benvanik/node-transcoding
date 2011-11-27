#include "filewriter.h"

using namespace transcoding;
using namespace transcoding::io;

FileWriter::FileWriter(Handle<Object> source) :
    IOWriter(source) {
  HandleScope scope;
  TC_LOG_D("FileWriter::FileWriter(%s)\n", *String::Utf8Value(source));

  this->path = *String::Utf8Value(source);
}

FileWriter::~FileWriter() {
  TC_LOG_D("FileWriter::~FileWriter()\n");
}

int FileWriter::Open() {
  AVIOContext* s = NULL;
  int ret = avio_open(&s, this->path.c_str(), AVIO_WRONLY);
  if (ret) {
    TC_LOG_D("FileWriter::Open(): failed (%d)\n", ret);
    return ret;
  }
  TC_LOG_D("FileWriter::Open()\n");
  this->context = s;
  return 0;
}

void FileWriter::Close() {
  TC_LOG_D("FileWriter::Close()\n");
  avio_close(this->context);
  this->context = NULL;
}
