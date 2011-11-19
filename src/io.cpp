#include "io.h"

AVFormatContext* createFileInputContext(const char* path, int* pret) {
  AVFormatContext* ctx = NULL;
  int ret = 0;
  *pret = 0;

  ret = avformat_open_input(&ctx, path, NULL, NULL);
  if (ret < 0) {
    goto CLEANUP;
  }

  ret = av_find_stream_info(ctx);
  if (ret < 0) {
    goto CLEANUP;
  }

  return ctx;

CLEANUP:
  if (ctx) {
    avformat_free_context(ctx);
  }
  *pret = ret;
  return NULL;
}

AVFormatContext* transcode::createInputContext(
    Handle<Object> source, int* pret) {
  HandleScope scope;
  *pret = 0;

  if (source->IsStringObject()) {
    String::AsciiValue asciiSource(source);
    return createFileInputContext(*asciiSource, pret);
  } else {
    // Not supported
    *pret = -1;
    return NULL;
  }
}

AVFormatContext* transcode::createOutputContext(
    Handle<Object> target, int* pret) {
  HandleScope scope;
  *pret = 0;
  return NULL;
}

void transcode::cleanupContext(AVFormatContext* ctx) {
  if (ctx) {
    avformat_free_context(ctx);
  }
}
