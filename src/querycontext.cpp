#include "querycontext.h"

using namespace transcoding;
using namespace transcoding::io;

QueryContext::QueryContext(IOReader* input) :
    input(input),
    ictx(NULL) {
}

QueryContext::~QueryContext() {
  if (this->ictx) {
    avformat_free_context(this->ictx);
  }

  IOHandle::CloseWhenDone(this->input);
}

int QueryContext::Execute() {
  int ret = 0;

  // Grab contexts
  AVFormatContext* ictx = NULL;
  if (!ret) {
    ictx = createInputContext(this->input, &ret);
    if (ret) {
      if (ictx) {
        avformat_free_context(ictx);
      }
      ictx = NULL;
    }
  }

  if (!ret) {
    this->ictx = ictx;
  } else {
    if (ictx) {
      avformat_free_context(ictx);
    }
    ictx = NULL;
  }

  return ret;
}
