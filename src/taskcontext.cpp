#include "taskcontext.h"

using namespace transcode;

TaskContext::TaskContext(IOHandle* input, IOHandle* output, Profile* profile) :
    running(true), abort(false), err(0),
    input(input), output(output), profile(profile),
    ictx(NULL), octx(NULL) {
  pthread_mutex_init(&this->lock, NULL);

  memset(&this->progress, 0, sizeof(this->progress));
}

TaskContext::~TaskContext() {
  assert(!this->running);

  pthread_mutex_destroy(&this->lock);

  if (this->ictx) {
    avformat_free_context(this->ictx);
  }
  if (this->octx) {
    avformat_free_context(this->octx);
  }

  delete this->input;
  delete this->output;
  delete this->profile;
}

Progress TaskContext::GetProgress() {
  pthread_mutex_lock(&this->lock);
  Progress progress = this->progress;
  pthread_mutex_unlock(&this->lock);
  return progress;
}

void TaskContext::Abort() {
  pthread_mutex_lock(&this->lock);
  this->abort = true;
  pthread_mutex_unlock(&this->lock);
}

int TaskContext::Prepare() {
  return 0;
}

bool TaskContext::Pump(int* pret) {
  return true;
}
