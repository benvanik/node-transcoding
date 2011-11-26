#include "querycontext.h"

using namespace transcoding;
using namespace transcoding::io;

QueryContext::QueryContext(IOReader* input) :
    running(false), abort(false), err(0),
    input(input),
    ictx(NULL) {
  pthread_mutex_init(&this->lock, NULL);
}

QueryContext::~QueryContext() {
  assert(!this->running);

  pthread_mutex_destroy(&this->lock);

  if (this->ictx) {
    avformat_free_context(this->ictx);
  }

  this->input->Close();

  delete this->input;
}

void QueryContext::Abort() {
  pthread_mutex_lock(&this->lock);
  this->abort = true;
  pthread_mutex_unlock(&this->lock);
}

int QueryContext::Execute() {
  int ret = 0;

  if (this->abort) {
    return AVERROR_EXIT;
  }

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
