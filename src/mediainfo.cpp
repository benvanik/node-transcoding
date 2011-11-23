#include "mediainfo.h"

using namespace transcoding;
using namespace v8;

Handle<Object> transcoding::createMediaInfo(AVFormatContext* ctx, bool encoding) {
  HandleScope scope;

  // Lop off just the first container name
  // e.g., mov,mp4,m4a.... -> mov
  char container[256];
  strcpy(container, encoding ? ctx->oformat->name : ctx->iformat->name);
  char* containerHead = strchr(container, ',');
  if (containerHead) {
    *containerHead = 0;
  }

  Local<Object> result = Object::New();
  result->Set(String::New("container"), String::New(container));
  if (ctx->duration != AV_NOPTS_VALUE) {
    result->Set(String::New("duration"),
        Number::New(ctx->duration / (double)AV_TIME_BASE));
  }
  if (ctx->start_time != AV_NOPTS_VALUE) {
    result->Set(String::New("start"),
        Number::New(ctx->start_time / (double)AV_TIME_BASE));
  }
  if (ctx->bit_rate) {
    result->Set(String::New("bitrate"),
        Number::New(ctx->bit_rate));
  }

  Local<Array> streams = Array::New();
  result->Set(String::New("streams"), streams);
  for (int n = 0; n < ctx->nb_streams; n++) {
    AVStream* st = ctx->streams[n];
    AVCodecContext* sc = st->codec;

    Local<Object> stream = Object::New();
    streams->Set(streams->Length(), stream);

    switch (sc->codec_type) {
      case AVMEDIA_TYPE_VIDEO: do {
        stream->Set(String::New("type"), String::New("video"));
        Local<Object> resolution = Object::New();
        resolution->Set(String::New("width"), Number::New(sc->width));
        resolution->Set(String::New("height"), Number::New(sc->height));
        stream->Set(String::New("resolution"), resolution);
        if (st->avg_frame_rate.den && st->avg_frame_rate.num) {
          stream->Set(String::New("fps"),
              Number::New(av_q2d(st->avg_frame_rate)));
        } else if (st->r_frame_rate.den && st->r_frame_rate.num) {
          stream->Set(String::New("fps"),
              Number::New(av_q2d(st->r_frame_rate)));
        }
        } while(0); break;
      case AVMEDIA_TYPE_AUDIO: do {
        stream->Set(String::New("type"), String::New("audio"));
        stream->Set(String::New("channels"), Number::New(sc->channels));
        stream->Set(String::New("sampleRate"), Number::New(sc->sample_rate));
        const char* sampleFormat = "unknown";
        switch (sc->sample_fmt) {
          default:
          case AV_SAMPLE_FMT_NONE:
            break;
          case AV_SAMPLE_FMT_U8:
            sampleFormat = "u8";
            break;
          case AV_SAMPLE_FMT_S16:
            sampleFormat = "s16";
            break;
          case AV_SAMPLE_FMT_S32:
            sampleFormat = "s32";
            break;
          case AV_SAMPLE_FMT_FLT:
            sampleFormat = "flt";
            break;
          case AV_SAMPLE_FMT_DBL:
            sampleFormat = "dbl";
            break;
        }
        stream->Set(String::New("sampleFormat"), String::New(sampleFormat));
        } while(0); break;
      case AVMEDIA_TYPE_SUBTITLE:
        stream->Set(String::New("type"), String::New("subtitle"));
        break;
      default:
        // Other - ignore
        continue;
    }

    AVCodec* codec = false ?
        avcodec_find_encoder(sc->codec_id) :
        avcodec_find_decoder(sc->codec_id);
    if (codec) {
      stream->Set(String::New("codec"), String::New(codec->name));
      if (sc->profile != FF_PROFILE_UNKNOWN) {
        const char* codecProfile = av_get_profile_name(codec, sc->profile);
        if (codecProfile) {
          stream->Set(String::New("profile"), String::New(codecProfile));
        }
        stream->Set(String::New("profileId"), Number::New(sc->profile));
        stream->Set(String::New("profileLevel"), Number::New(sc->level));
      }
    }

    if (sc->bit_rate) {
      stream->Set(String::New("bitrate"), Number::New(sc->bit_rate));
    }

    AVDictionaryEntry* lang = av_dict_get(st->metadata, "language", NULL, 0);
    if (lang) {
      // Not in ISO format - often 'eng' or something
      // TODO: convert language to something usable
      if (strcmp(lang->value, "und") != 0) {
        stream->Set(String::New("language"), String::New(lang->value));
      }
    }
  }

  return scope.Close(result);
}
