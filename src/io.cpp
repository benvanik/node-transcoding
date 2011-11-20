#include "io.h"

using namespace transcode;

IODescriptor::IODescriptor(Handle<Object> source) :
    filename(NULL) {
  if (source->IsStringObject()) {
    String::AsciiValue asciiSource(source);
    this->filename = strdup(*asciiSource);
  }
}

IODescriptor::~IODescriptor() {
  if (this->filename) {
    free(this->filename);
    this->filename = NULL;
  }
}

InputDescriptor::InputDescriptor(Handle<Object> source) :
    IODescriptor(source) {
}

InputDescriptor::~InputDescriptor() {
}

OutputDescriptor::OutputDescriptor(Handle<Object> target) :
    IODescriptor(target) {
}

OutputDescriptor::~OutputDescriptor() {
}

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
    InputDescriptor* descr, int* pret) {
  *pret = 0;

  if (descr->filename) {
    return createFileInputContext(descr->filename, pret);
  } else {
    // Not supported
    *pret = -1;
    return NULL;
  }
}

AVFormatContext* createFileOutputContext(const char* path, int* pret) {
  AVFormatContext* ctx = NULL;
  int ret = 0;
  *pret = 0;

  ctx = avformat_alloc_context();
  if (!ctx) {
    ret = AVERROR_NOMEM;
    goto CLEANUP;
  }

  ret = url_fopen(&ctx->pb, path, URL_WRONLY);
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

AVFormatContext* transcode::createOutputContext(
    OutputDescriptor* descr, int* pret) {
  *pret = 0;

  if (descr->filename) {
    return createFileOutputContext(descr->filename, pret);
  } else {
    // Not supported
    *pret = -1;
    return NULL;
  }
}

void transcode::cleanupContext(AVFormatContext* ctx) {
  if (ctx) {
    avformat_free_context(ctx);
  }
}
