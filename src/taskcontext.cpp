#include "taskcontext.h"

using namespace transcoding;
using namespace transcoding::io;

// Number of packets to keep in the FIFO
#define FIFO_MIN_COUNT 64

TaskContext::TaskContext(IOReader* input, Profile* profile,
    TaskOptions* options) :
    input(input), profile(profile), options(options),
    ictx(NULL), bitStreamFilter(NULL), fifo(NULL), doneReading(false) {
  TC_LOG_D("TaskContext::TaskContext()\n");
}

TaskContext::~TaskContext() {
  TC_LOG_D("TaskContext::~TaskContext()\n");

  if (this->fifo) {
    delete this->fifo;
  }

  if (this->bitStreamFilter) {
    av_bitstream_filter_close(this->bitStreamFilter);
  }

  if (this->ictx) {
    avformat_free_context(this->ictx);
  }

  delete this->profile;
  delete this->options;

  IOHandle::CloseWhenDone(this->input);
}

int TaskContext::PrepareInput() {
  TC_LOG_D("TaskContext::PrepareInput()\n");
  int ret = 0;

  AVFormatContext* ictx = createInputContext(this->input, &ret);
  if (ret) {
    TC_LOG_D("TaskContext::PrepareInput(): failed createInputContext (%d)\n",
        ret);
  }

  if (!ret) {
    this->ictx = ictx;
  } else {
    if (ictx) {
      avformat_free_context(ictx);
    }
    ictx = NULL;
  }

  TC_LOG_D("TaskContext::PrepareInput() = %d\n", ret);
  return ret;
}

int TaskContext::PrepareOutput() {
  TC_LOG_D("TaskContext::PrepareOutput()\n");
  int ret = 0;

  AVFormatContext* octx = avformat_alloc_context();

  // Setup output container
  if (!ret) {
    AVOutputFormat* ofmt = av_guess_format(
        profile->container.c_str(), NULL, NULL);
    if (ofmt) {
      octx->oformat = ofmt;
    } else {
      ret = AVERROR_NOFMT;
      TC_LOG_D("TaskContext::PrepareOutput(): oformat %s not found (%d)\n",
          profile->container.c_str(), ret);
    }
    octx->duration    = ictx->duration;
    octx->start_time  = ictx->start_time;
    octx->bit_rate    = ictx->bit_rate;
  }

  // Setup input FIFO
  if (!ret) {
    this->fifo = new PacketFifo(ictx->nb_streams);
  }

  // Setup streams
  if (!ret) {
    for (int n = 0; n < ictx->nb_streams; n++) {
      AVStream* stream = ictx->streams[n];
      switch (stream->codec->codec_type) {
      case CODEC_TYPE_VIDEO:
      case CODEC_TYPE_AUDIO:
        stream->discard = AVDISCARD_NONE;
        this->AddOutputStreamCopy(octx, stream, &ret);
        break;
      // TODO: subtitles
      case CODEC_TYPE_SUBTITLE:
      default:
        stream->discard = AVDISCARD_ALL;
        break;
      }
      if (ret) {
        TC_LOG_D("TaskContext::PrepareOutput(): failed stream add (%d)\n", ret);
        break;
      }
    }
  }

  // Scan video streams to see if we need to set up the bitstream filter for
  // fixing h264 in mpegts
  // This is equivalent to the -vbsf h264_mp4toannexb option
  if (!ret && strcmp(octx->oformat->name, "mpegts") == 0) {
    for (int n = 0; n < octx->nb_streams; n++) {
      AVStream* stream = octx->streams[n];
      if (stream->codec->codec_id == CODEC_ID_H264) {
        TC_LOG_D("TaskContext::PrepareOutput(): h264_mp4toannexb on stream "
            "%d\n", n);
        AVBitStreamFilterContext* bsfc =
            av_bitstream_filter_init("h264_mp4toannexb");
        if (!bsfc) {
          ret = AVERROR_BSF_NOT_FOUND;
          TC_LOG_D("TaskContext::PrepareOutput(): h264_mp4toannexb missing\n");
        } else {
          bsfc->next = this->bitStreamFilter;
          this->bitStreamFilter = bsfc;
        }
      }
    }
  }

  if (!ret) {
    this->octx = octx;
  } else {
    if (octx) {
      avformat_free_context(octx);
    }
    octx = NULL;
  }

  TC_LOG_D("TaskContext::PrepareOutput() = %d\n", ret);
  return ret;
}

AVStream* TaskContext::AddOutputStreamCopy(AVFormatContext* octx,
    AVStream* istream, int* pret) {
  int ret = 0;
  AVStream* ostream = NULL;
  AVCodec* codec = NULL;
  AVCodecContext* icodec = NULL;
  AVCodecContext* ocodec = NULL;

  codec = avcodec_find_encoder(istream->codec->codec_id);
  if (!codec) {
    ret = AVERROR_ENCODER_NOT_FOUND;
    goto CLEANUP;
  }

  ostream = av_new_stream(octx, 0);
  if (!ostream) {
    ret = AVERROR_NOMEM;
    goto CLEANUP;
  }

  icodec = istream->codec;
  ocodec = ostream->codec;

  // May not do anything
  ostream->stream_copy = 1;

  avcodec_get_context_defaults3(ocodec, codec);

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

  // Input stream may not end on frame boundaries
  if (codec->capabilities & CODEC_CAP_TRUNCATED) {
    icodec->flags |= CODEC_FLAG_TRUNCATED;
  }

  // Try to write output headers at the start
  if (octx->oformat->flags & AVFMT_GLOBALHEADER) {
    ocodec->flags |= CODEC_FLAG_GLOBAL_HEADER;
  }

  ocodec->extradata               = (uint8_t*)av_mallocz(
      icodec->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE);
  if (!ocodec->extradata) {
    ret = AVERROR(ENOMEM);
    goto CLEANUP;
  }
  memcpy(ocodec->extradata, icodec->extradata, icodec->extradata_size);
  ocodec->extradata_size          = icodec->extradata_size;

  // Code from avconv, but doesn't seem to work all the time - for now just
  // copy the timebase
  ocodec->time_base               = istream->time_base;
  if (av_q2d(icodec->time_base) * icodec->ticks_per_frame >
     av_q2d(istream->time_base) && av_q2d(istream->time_base) < 1 / 1000.0) {
   ocodec->time_base             = icodec->time_base;
   ocodec->time_base.num         *= icodec->ticks_per_frame;
  } else {
   ocodec->time_base             = istream->time_base;
  }

  switch (icodec->codec_type) {
  case CODEC_TYPE_VIDEO:
    ocodec->pix_fmt               = icodec->pix_fmt;
    ocodec->width                 = icodec->width;
    ocodec->height                = icodec->height;
    ocodec->has_b_frames          = icodec->has_b_frames;
    if (!ocodec->sample_aspect_ratio.num) {
      ocodec->sample_aspect_ratio = ostream->sample_aspect_ratio =
          istream->sample_aspect_ratio.num ? istream->sample_aspect_ratio :
          icodec->sample_aspect_ratio.num ? icodec->sample_aspect_ratio :
          (AVRational){0, 1};
    }
    if (octx->oformat->flags & AVFMT_GLOBALHEADER) {
      ocodec->flags               |= CODEC_FLAG_GLOBAL_HEADER;
    }
    break;
  case CODEC_TYPE_AUDIO:
    ocodec->channel_layout        = icodec->channel_layout;
    ocodec->sample_rate           = icodec->sample_rate;
    ocodec->sample_fmt            = icodec->sample_fmt;
    ocodec->channels              = icodec->channels;
    ocodec->frame_size            = icodec->frame_size;
    ocodec->audio_service_type    = icodec->audio_service_type;
    if ((icodec->block_align == 1 && icodec->codec_id == CODEC_ID_MP3) ||
        icodec->codec_id == CODEC_ID_AC3) {
      ocodec->block_align         = 0;
    } else {
      ocodec->block_align         = icodec->block_align;
    }
    break;
  case CODEC_TYPE_SUBTITLE:
    // ?
    ocodec->width                 = icodec->width;
    ocodec->height                = icodec->height;
    break;
  default:
    break;
  }

  // TODO: threading settings
  ocodec->thread_count = 2;

  return ostream;

CLEANUP:
  *pret = ret;
  // TODO: cleanup ostream?
  return NULL;
}

bool TaskContext::NextPacket(int* pret, Progress* progress, AVPacket& packet) {
  TC_LOG_D("TaskContext::NextPacket()\n");

  AVFormatContext* ictx = this->ictx;

  int ret = 0;

  // Read a few packets to fill our queue before we process
  if (!this->doneReading) {
    while (this->fifo->GetCount() <= FIFO_MIN_COUNT) {
      AVPacket readPacket;
      int done = av_read_frame(ictx, &readPacket);
      if (done) {
        TC_LOG_D("TaskContext::NextPacket(): done/failed to read frame (%d)\n",
            ret);
        *pret = 0;
        this->doneReading = true;
        break;
      } else {
        double timestamp = readPacket.pts *
            (double)ictx->streams[readPacket.stream_index]->time_base.num /
            (double)ictx->streams[readPacket.stream_index]->time_base.den;
        this->fifo->QueuePacket(readPacket.stream_index, readPacket, timestamp);
      }
    }
  }
  if (this->doneReading && !this->fifo->GetCount()) {
    return true;
  }

  // Grab the next packet
  if (!this->fifo->DequeuePacket(packet)) {
    return true;
  }
  double timestamp = packet.pts *
      (double)ictx->streams[packet.stream_index]->time_base.num /
      (double)ictx->streams[packet.stream_index]->time_base.den;

  AVStream* stream = ictx->streams[packet.stream_index];

  // Ignore if we don't care about this stream
  if (stream->discard == AVDISCARD_ALL) {
    return false;
  }

  ret = av_dup_packet(&packet);
  if (ret) {
    TC_LOG_D("TaskContext::NextPacket(): failed to duplicate packet (%d)\n",
        ret);
    av_free_packet(&packet);
    *pret = ret;
    return true;
  }

  AVBitStreamFilterContext* bsfc = this->bitStreamFilter;
  while (bsfc) {
    AVPacket newPacket = packet;
    ret = av_bitstream_filter_filter(bsfc, stream->codec, NULL,
        &newPacket.data, &newPacket.size, packet.data, packet.size,
        packet.flags & AV_PKT_FLAG_KEY);
    if (ret > 0) {
       av_free_packet(&packet);
       newPacket.destruct = av_destruct_packet;
       ret = 0;
    } else if (ret < 0) {
       // Error!
       break;
    }
    packet = newPacket;
    bsfc = bsfc->next;
  }
  if (ret) {
    TC_LOG_D("TaskContext::NextPacket(): failed to filter packet (%d)\n", ret);
    *pret = ret;
    av_free_packet(&packet);
    return true;
  }

  // Update progress (only on success)
  if (!ret) {
    progress->timestamp = timestamp;
  }

  if (ret) {
    TC_LOG_D("TaskContext::NextPacket() = %d\n", ret);
  }
  *pret = ret;
  return false;
}

bool TaskContext::WritePacket(int* pret, AVPacket& packet) {
  TC_LOG_D("TaskContext::WritePacket()\n");

  AVFormatContext* ictx = this->ictx;
  AVFormatContext* octx = this->octx;

  int ret = 0;

  ret = av_interleaved_write_frame(octx, &packet);
  if (ret < 0) {
    TC_LOG_D("TaskContext::WritePacket(): could not write frame of "
        "stream (%d)\n", ret);
  } else if (ret > 0) {
    TC_LOG_D("TaskContext::WritePacket(): end of stream requested (%d)\n", ret);
    av_free_packet(&packet);
    *pret = ret;
    return true;
  }

  if (ret) {
    TC_LOG_D("TaskContext::WritePacket() = %d\n", ret);
  }
  *pret = ret;
  return false;
}

bool TaskContext::Pump(int* pret, Progress* progress) {
  TC_LOG_D("TaskContext::Pump()\n");

  AVPacket packet;
  if (this->NextPacket(pret, progress, packet)) {
    TC_LOG_D("TaskContext::Pump() = %d\n", *pret);
    return true;
  }

  if (this->WritePacket(pret, packet)) {
    TC_LOG_D("TaskContext::Pump() = %d\n", *pret);
    av_free_packet(&packet);
    return true;
  }

  av_free_packet(&packet);

  return false;
}

void TaskContext::End() {
  TC_LOG_D("TaskContext::End()\n");

  AVFormatContext* ictx = this->ictx;
  AVFormatContext* octx = this->octx;

  av_write_trailer(octx);
  avio_flush(octx->pb);
}

SingleFileTaskContext::SingleFileTaskContext(io::IOReader* input,
    io::IOWriter* output, Profile* profile, TaskOptions* options) :
    TaskContext(input, profile, options),
    output(output) {
  TC_LOG_D("SingleFileTaskContext::SingleFileTaskContext()\n");
}

SingleFileTaskContext::~SingleFileTaskContext() {
  TC_LOG_D("SingleFileTaskContext::~SingleFileTaskContext()\n");

  if (this->octx) {
    avformat_free_context(this->octx);
  }
  IOHandle::CloseWhenDone(this->output);
}

int SingleFileTaskContext::PrepareOutput() {
  TC_LOG_D("SingleFileTaskContext::PrepareOutput()\n");

  int ret = TaskContext::PrepareOutput();

  AVFormatContext* octx = this->octx;

  // Open output
  if (!ret) {
    ret = this->output->Open();
    if (ret) {
      TC_LOG_D("SingleFileTaskContext::PrepareOutput(): failed open (%d)\n",
          ret);
    }
  }
  if (!ret) {
    octx->pb = this->output->context;
    if (!octx->pb) {
      ret = AVERROR_NOENT;
      TC_LOG_D("SingleFileTaskContext::PrepareOutput(): no pb (%d)\n", ret);
    }
  }

  // Write header
  if (!ret) {
    ret = avformat_write_header(octx, NULL);
    if (ret) {
      TC_LOG_D("SingleFileTaskContext::PrepareOutput(): failed write_header "
          " (%d)\n", ret);
    }
  }

  TC_LOG_D("SingleFileTaskContext::PrepareOutput() = %d\n", ret);
  return ret;
}

LiveStreamingTaskContext::LiveStreamingTaskContext(
    io::IOReader* input, Profile* profile, TaskOptions* options) :
    TaskContext(input, profile, options) {
  TC_LOG_D("LiveStreamingTaskContext::LiveStreamingTaskContext()\n");

  this->playlist = new hls::Playlist(
      options->liveStreaming->path, options->liveStreaming->name,
      options->liveStreaming->segmentDuration,
      options->liveStreaming->allowCaching);
}

LiveStreamingTaskContext::~LiveStreamingTaskContext() {
  TC_LOG_D("LiveStreamingTaskContext::~LiveStreamingTaskContext()\n");

  delete this->playlist;
}

int LiveStreamingTaskContext::PrepareOutput() {
  TC_LOG_D("LiveStreamingTaskContext::PrepareOutput()\n");

  int ret = 0;

  if (ret) {
    TC_LOG_D("LiveStreamingTaskContext::PrepareOutput() = %d\n", ret);
  }
  return ret;
}

bool LiveStreamingTaskContext::Pump(int* pret, Progress* progress) {
  TC_LOG_D("LiveStreamingTaskContext::Pump()\n");

  AVPacket packet;
  if (this->NextPacket(pret, progress, packet)) {
    TC_LOG_D("LiveStreamingTaskContext::Pump() = %d\n", *pret);
    return true;
  }

  // TODO: switch segment/etc based on packet timeestamp/duration

  if (this->WritePacket(pret, packet)) {
    TC_LOG_D("LiveStreamingTaskContext::Pump() = %d\n", *pret);
    av_free_packet(&packet);
    return true;
  }

  av_free_packet(&packet);

  return false;
}
