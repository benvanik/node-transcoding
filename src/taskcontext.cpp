#include "taskcontext.h"

using namespace transcoding;
using namespace transcoding::io;

TaskContext::TaskContext(IOReader* input, IOWriter* output, Profile* profile) :
    input(input), output(output), profile(profile),
    ictx(NULL), octx(NULL), bitStreamFilter(NULL) {
  TC_LOG_D("TaskContext::TaskContext()\n");
}

TaskContext::~TaskContext() {
  TC_LOG_D("TaskContext::~TaskContext()\n");

  if (this->bitStreamFilter) {
    av_bitstream_filter_close(this->bitStreamFilter);
  }

  if (this->ictx) {
    avformat_free_context(this->ictx);
  }
  if (this->octx) {
    avformat_free_context(this->octx);
  }

  delete this->profile;

  IOHandle::CloseWhenDone(this->input);
  IOHandle::CloseWhenDone(this->output);
}

int TaskContext::Prepare() {
  TC_LOG_D("TaskContext::Prepare()\n");
  int ret = 0;

  // Grab contexts
  AVFormatContext* ictx = NULL;
  AVFormatContext* octx = NULL;
  if (!ret) {
    ictx = createInputContext(this->input, &ret);
    if (ret) {
      TC_LOG_D("TaskContext::Prepare(): failed createInputContext (%d)\n",
          ret);
      if (ictx) {
        avformat_free_context(ictx);
      }
      ictx = octx = NULL;
    }
  }
  if (!ret) {
    octx = createOutputContext(this->output, &ret);
    if (ret) {
      TC_LOG_D("TaskContext::Prepare(): failed createOutputContext (%d)\n",
          ret);
      if (ictx) {
        avformat_free_context(ictx);
      }
      if (octx) {
        avformat_free_context(octx);
      }
      ictx = octx = NULL;
    }
  }

  // Setup output container
  if (!ret) {
    AVOutputFormat* ofmt = av_guess_format(
        profile->container.c_str(), NULL, NULL);
    if (ofmt) {
      octx->oformat = ofmt;
    } else {
      ret = AVERROR_NOFMT;
      TC_LOG_D("TaskContext::Prepare(): oformat %s not found (%d)\n",
          profile->container.c_str(), ret);
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
        TC_LOG_D("TaskContext::Prepare(): failed stream add (%d)\n", ret);
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
        TC_LOG_D("TaskContext::Prepare(): h264_mp4toannexb on stream %d\n", n);
        AVBitStreamFilterContext* bsfc =
            av_bitstream_filter_init("h264_mp4toannexb");
        if (!bsfc) {
          ret = AVERROR_BSF_NOT_FOUND;
          TC_LOG_D("TaskContext::Prepare(): h264_mp4toannexb not found\n");
        } else {
          bsfc->next = this->bitStreamFilter;
          this->bitStreamFilter = bsfc;
        }
      }
    }
  }

  // Write header
  if (!ret) {
    ret = avformat_write_header(octx, NULL);
    if (ret) {
      TC_LOG_D("TaskContext::Prepare(): failed write_header (%d)\n", ret);
    }
  }

  if (!ret) {
    this->ictx = ictx;
    this->octx = octx;
  } else {
    if (ictx) {
      avformat_free_context(ictx);
    }
    if (octx) {
      avformat_free_context(octx);
    }
    ictx = octx = NULL;
  }

  TC_LOG_D("TaskContext::Prepare() = %d\n", ret);
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

bool TaskContext::Pump(int* pret, Progress* progress) {
  //TC_LOG_D("TaskContext::Pump()\n");

  AVFormatContext* ictx = this->ictx;
  AVFormatContext* octx = this->octx;

  int ret = 0;

  AVPacket packet;
  int done = av_read_frame(ictx, &packet);
  if (done) {
    TC_LOG_D("TaskContext::Pump(): done/failed to read frame (%d)\n", ret);
    *pret = 0;
    return true;
  }

  AVStream* stream = ictx->streams[packet.stream_index];

  // Ignore if we don't care about this stream
  if (stream->discard == AVDISCARD_ALL) {
    return false;
  }

  ret = av_dup_packet(&packet);
  if (ret) {
    TC_LOG_D("TaskContext::Pump(): failed to duplicate packet (%d)\n", ret);
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
    TC_LOG_D("TaskContext::Pump(): failed to filter packet (%d)\n", ret);
    *pret = ret;
    av_free_packet(&packet);
    return true;
  }

  ret = av_interleaved_write_frame(octx, &packet);
  if (ret < 0) {
    TC_LOG_D("TaskContext::Pump(): could not write frame of stream (%d)\n",
        ret);
  } else if (ret > 0) {
    TC_LOG_D("TaskContext::Pump(): end of stream requested (%d)\n", ret);
    av_free_packet(&packet);
    *pret = ret;
    return true;
  }

  av_free_packet(&packet);

  // Update progress (only on success)
  if (!ret) {
    progress->timestamp = packet.pts / (double)stream->time_base.den;
  }

  if (ret) {
    TC_LOG_D("TaskContext::Pump() = %d\n", ret);
  }
  *pret = ret;
  return false;
}

void TaskContext::End() {
  TC_LOG_D("TaskContext::End()\n");

  AVFormatContext* ictx = this->ictx;
  AVFormatContext* octx = this->octx;

  av_write_trailer(octx);
  avio_flush(octx->pb);
}
