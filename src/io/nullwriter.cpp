#include "nullwriter.h"

using namespace transcoding;
using namespace transcoding::io;

NullWriter::NullWriter() :
    IOWriter(Object::New()) {
  HandleScope scope;
  TC_LOG_D("NullWriter::NullWriter()\n");
}

NullWriter::~NullWriter() {
  TC_LOG_D("NullWriter::~NullWriter()\n");
}

int NullWriter::Open() {
  AVIOContext* s = NULL;
  int ret = avio_open_dyn_buf(&s);
  if (ret) {
    TC_LOG_D("NullWriter::Open(): failed (%d)\n", ret);
    return ret;
  }
  TC_LOG_D("NullWriter::Open()\n");
  this->context = s;
  return 0;
}

void NullWriter::Close() {
  TC_LOG_D("NullWriter::Close()\n");
  uint8_t* buffer = NULL;
  int size = avio_close_dyn_buf(this->context, &buffer);
  av_free(buffer);
  this->context = NULL;
}
