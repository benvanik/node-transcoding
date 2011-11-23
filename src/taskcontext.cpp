#include "taskcontext.h"

using namespace transcode;

TaskContext::TaskContext(IOHandle* input, IOHandle* output, Profile* profile) :
    running(false), abort(false), err(0),
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

  printf("taskcontext dtor\n");
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
  int ret = 0;

  // Grab contexts
  AVFormatContext* ictx = NULL;
  AVFormatContext* octx = NULL;
  if (!ret) {
    ictx = createInputContext(this->input, &ret);
    if (ret) {
      avformat_free_context(ictx);
      avformat_free_context(octx);
      ictx = octx = NULL;
    }
  }
  if (!ret) {
    octx = createOutputContext(this->output, &ret);
    if (ret) {
      avformat_free_context(ictx);
      avformat_free_context(octx);
      ictx = octx = NULL;
    }
  }

  // Setup output container
  if (!ret) {
    AVOutputFormat* ofmt = av_guess_format("mov", NULL, NULL);
    if (ofmt) {
      octx->oformat = ofmt;
    } else {
      ret = AVERROR_NOFMT;
    }
    octx->duration    = ictx->duration;
    octx->start_time  = ictx->start_time;
    octx->bit_rate    = ictx->bit_rate;
  }

  // Setup streams
  if (!ret) {
    for (int n = 0; n < ictx->nb_streams; n++) {
      AVStream* stream = ictx->streams[n];
      switch (stream->codec->codec_type) {
      case CODEC_TYPE_VIDEO:
      case CODEC_TYPE_AUDIO:
      case CODEC_TYPE_SUBTITLE:
        stream->discard = AVDISCARD_NONE;
        //AddOutputStreamCopy(octx, stream, &ret);
        break;
      default:
        stream->discard = AVDISCARD_ALL;
        break;
      }
      if (ret) {
        break;
      }
    }
  }

  // Write header
  if (!ret) {
    ret = avformat_write_header(octx, NULL);
  }

  if (!ret) {
    this->ictx = ictx;
    this->octx = octx;
  } else {
    avformat_free_context(ictx);
    avformat_free_context(octx);
    ictx = octx = NULL;
  }

  return ret;
}

bool TaskContext::Pump(int* pret) {
  return true;
}

void TaskContext::End() {
  AVFormatContext* ictx = this->ictx;
  AVFormatContext* octx = this->octx;

  av_write_trailer(octx);
  avio_flush(octx->pb);

  this->output->Close(octx->pb);
  this->input->Close(ictx->pb);
}
