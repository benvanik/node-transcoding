#include "processor.h"

using namespace transcode;

typedef struct ProcessorReq_t {
  uv_work_t           request;
  Processor*          processor;
  Persistent<Object>  obj;
} ProcessorReq;

typedef struct ProcessorAsyncBegin_t {
  Processor*          processor;
  AVFormatContext*    ictx;
  AVFormatContext*    octx;
} ProcessorAsyncBegin;

typedef struct ProcessorAsyncProgress_t {
  Processor*          processor;
  Progress            progress;
} ProcessorAsyncProgress;

typedef struct ProcessorAsyncError_t {
  Processor*          processor;
  int                 err;
} ProcessorAsyncError;

typedef struct ProcessorAsyncEnd_t {
  Processor*          processor;
} ProcessorAsyncEnd;

ProcessorOptions::ProcessorOptions(Handle<Object> source) {
  HandleScope scope;
}

ProcessorOptions::~ProcessorOptions() {
}

Processor::Processor() :
    running(false), abort(false), sink(NULL), ictx(NULL), octx(NULL) {
  pthread_mutex_init(&this->lock, NULL);

  memset(&this->progress, 0, sizeof(this->progress));
}

Processor::~Processor() {
  cleanupContext(this->ictx);
  this->ictx = NULL;
  cleanupContext(this->octx);
  this->octx = NULL;
  if (this->input) {
    delete this->input;
  }
  if (this->output) {
    delete this->output;
  }
  if (this->options) {
    delete this->options;
  }
  pthread_mutex_destroy(&this->lock);
  this->sink = NULL;
}

void Processor::SetSink(ProcessorSink* sink) {
  assert(!this->sink);
  assert(sink);
  this->sink = sink;
}

void Processor::Execute(InputDescriptor* input, OutputDescriptor* output,
    ProcessorOptions* options, Handle<Object> obj) {
  HandleScope scope;

  this->input = input;
  this->output = output;
  this->options = options;

  this->running = true;

  ProcessorReq* req = new ProcessorReq();
  req->request.data = req;
  req->processor = this;
  req->obj = Persistent<Object>::New(obj);

  int status = uv_queue_work(uv_default_loop(),
      &req->request, ThreadWorker, ThreadWorkerComplete);
  assert(status == 0);
}

AVStream* Processor::AddOutputStreamCopy(AVFormatContext* octx,
    AVStream* istream, int* pret) {
  int ret = 0;
  AVStream* ostream = NULL;
  AVCodecContext* icodec = NULL;
  AVCodecContext* ocodec = NULL;

  ostream = av_new_stream(octx, 0);
  if (!ostream) {
    ret = AVERROR_NOMEM;
    goto CLEANUP;
  }

  icodec = istream->codec;
  ocodec = ostream->codec;

  ocodec->codec_id                = icodec->codec_id;
  ocodec->codec_type              = icodec->codec_type;
  ocodec->codec_tag               = icodec->codec_tag;
  ocodec->profile                 = icodec->profile;
  ocodec->level                   = icodec->level;
  ocodec->bit_rate                = icodec->bit_rate;
  ocodec->bits_per_raw_sample     = icodec->bits_per_raw_sample;
  ocodec->chroma_sample_location  = icodec->chroma_sample_location;
  ocodec->rc_max_rate             = icodec->rc_max_rate;
  ocodec->rc_buffer_size          = icodec->rc_buffer_size;

  ocodec->extradata               = (uint8_t*)av_mallocz(
      icodec->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
  if (!ocodec->extradata) {
    ret = AVERROR(ENOMEM);
    goto CLEANUP;
  }
  memcpy(ocodec->extradata, icodec->extradata, icodec->extradata_size);
  ocodec->extradata_size          = icodec->extradata_size;

  if (av_q2d(icodec->time_base) * icodec->ticks_per_frame >
      av_q2d(istream->time_base) && av_q2d(istream->time_base) < 1 / 1000.0) {
    ocodec->time_base             = icodec->time_base;
    ocodec->time_base.num         *= icodec->ticks_per_frame;
  } else {
    ocodec->time_base             = istream->time_base;
  }

  switch (icodec->codec_type) {
  case CODEC_TYPE_VIDEO:
    ocodec->pix_fmt             = icodec->pix_fmt;
    ocodec->width               = icodec->width;
    ocodec->height              = icodec->height;
    ocodec->has_b_frames        = icodec->has_b_frames;
    if (!ocodec->sample_aspect_ratio.num) {
      ocodec->sample_aspect_ratio = ostream->sample_aspect_ratio =
          istream->sample_aspect_ratio.num ? istream->sample_aspect_ratio :
          icodec->sample_aspect_ratio.num ? icodec->sample_aspect_ratio :
          (AVRational){0, 1};
    }
    if (octx->oformat->flags & AVFMT_GLOBALHEADER) {
      ocodec->flags           |= CODEC_FLAG_GLOBAL_HEADER;
    }
    break;
  case CODEC_TYPE_AUDIO:
    ocodec->channel_layout      = icodec->channel_layout;
    ocodec->sample_rate         = icodec->sample_rate;
    ocodec->sample_fmt          = icodec->sample_fmt;
    ocodec->channels            = icodec->channels;
    ocodec->frame_size          = icodec->frame_size;
    ocodec->audio_service_type  = icodec->audio_service_type;
    if ((icodec->block_align == 1 && icodec->codec_id == CODEC_ID_MP3) ||
        icodec->codec_id == CODEC_ID_AC3) {
      ocodec->block_align       = 0;
    } else {
      ocodec->block_align       = icodec->block_align;
    }
    break;
  case CODEC_TYPE_SUBTITLE:
    // ?
    ocodec->width               = icodec->width;
    ocodec->height              = icodec->height;
    break;
  default:
    break;
  }

  return ostream;

CLEANUP:
  *pret = ret;
  // TODO: cleanup ostream?
  return NULL;
}

void Processor::ThreadWorker(uv_work_t* request) {
  ProcessorReq* req = static_cast<ProcessorReq*>(request->data);
  Processor* processor = req->processor;

  int ret = 0;

  // Grab contexts
  AVFormatContext* ictx = NULL;
  AVFormatContext* octx = NULL;
  if (!ret) {
    ictx = createInputContext(processor->input, &ret);
    if (ret) {
      cleanupContext(ictx);
      cleanupContext(octx);
    }
  }
  if (!ret) {
    octx = createOutputContext(processor->output, &ret);
    if (ret) {
      cleanupContext(ictx);
      cleanupContext(octx);
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
        AddOutputStreamCopy(octx, stream, &ret);
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

  // Stash away on the processor (they will be cleaned up on destruction)
  // Emit begin
  if (!ret) {
    pthread_mutex_lock(&processor->lock);
    processor->ictx = ictx;
    processor->octx = octx;
    pthread_mutex_unlock(&processor->lock);

    ProcessorAsyncBegin* asyncBegin = new ProcessorAsyncBegin();
    asyncBegin->processor = processor;
    asyncBegin->ictx = ictx;
    asyncBegin->octx = octx;
    NODE_ASYNC_SEND(asyncBegin, EmitBegin);
  }

  // Main pump
  if (!ret) {
    Progress progress;
    memset(&progress, 0, sizeof(progress));
    while (true) {
      // Copy progress (speeds up queries later on)
      // Also grab the current abort flag
      pthread_mutex_lock(&processor->lock);
      bool aborting = processor->abort;
      memcpy(&processor->progress, &progress, sizeof(progress));
      pthread_mutex_unlock(&processor->lock);

      // Emit progress event
      ProcessorAsyncProgress* asyncProgress = new ProcessorAsyncProgress();
      asyncProgress->processor = processor;
      memcpy(&asyncProgress->progress, &progress, sizeof(progress));
      NODE_ASYNC_SEND(asyncProgress, EmitProgress);

      // TODO: work
      break;

      if (!ret || aborting) {
        // Error occurred or abort requested - early exit
        break;
      }
    };
  }

  // Emit error
  if (ret) {
    ProcessorAsyncError* asyncError = new ProcessorAsyncError();
    asyncError->processor = processor;
    asyncError->err = ret;
    NODE_ASYNC_SEND(asyncError, EmitError);
  }

  // Emit end
  ProcessorAsyncEnd* asyncEnd = new ProcessorAsyncEnd();
  asyncEnd->processor = processor;
  NODE_ASYNC_SEND(asyncEnd, EmitEnd);
}

void Processor::ThreadWorkerComplete(uv_work_t* request) {
  ProcessorReq* req = static_cast<ProcessorReq*>(request->data);
  Processor* processor = req->processor;

  processor->running = false;

  req->obj.Dispose();
  delete req;
}

void Processor::EmitBegin(uv_async_t* handle, int status) {
  assert(status == 0);
  ProcessorAsyncBegin* req =
      static_cast<ProcessorAsyncBegin*>(handle->data);

  req->processor->sink->EmitBegin(req->ictx, req->octx);

  NODE_ASYNC_CLOSE(handle, EmitBeginClose);
}

void Processor::EmitBeginClose(uv_handle_t* handle) {
  ProcessorAsyncBegin* req =
      static_cast<ProcessorAsyncBegin*>(handle->data);
  handle->data = NULL;
}

void Processor::EmitProgress(uv_async_t* handle, int status) {
  assert(status == 0);
  ProcessorAsyncProgress* req =
      static_cast<ProcessorAsyncProgress*>(handle->data);

  req->processor->sink->EmitProgress(req->progress);

  NODE_ASYNC_CLOSE(handle, EmitProgressClose);
}

void Processor::EmitProgressClose(uv_handle_t* handle) {
  ProcessorAsyncProgress* req =
      static_cast<ProcessorAsyncProgress*>(handle->data);
  handle->data = NULL;
}

void Processor::EmitError(uv_async_t* handle, int status) {
  assert(status == 0);
  ProcessorAsyncError* req =
      static_cast<ProcessorAsyncError*>(handle->data);

  req->processor->sink->EmitError(req->err);

  NODE_ASYNC_CLOSE(handle, EmitErrorClose);
}

void Processor::EmitErrorClose(uv_handle_t* handle) {
  ProcessorAsyncError* req =
      static_cast<ProcessorAsyncError*>(handle->data);
  handle->data = NULL;
}

void Processor::EmitEnd(uv_async_t* handle, int status) {
  assert(status == 0);
  ProcessorAsyncEnd* req =
      static_cast<ProcessorAsyncEnd*>(handle->data);

  req->processor->sink->EmitEnd();

  NODE_ASYNC_CLOSE(handle, EmitEndClose);
}

void Processor::EmitEndClose(uv_handle_t* handle) {
  ProcessorAsyncEnd* req =
      static_cast<ProcessorAsyncEnd*>(handle->data);
  handle->data = NULL;
}

Progress Processor::GetProgress() {
  return this->progress;
}

void Processor::Abort() {
  pthread_mutex_lock(&this->lock);
  this->abort = true;
  pthread_mutex_unlock(&this->lock);
}
