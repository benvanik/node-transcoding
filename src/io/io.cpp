#include "io.h"

using namespace transcoding;
using namespace transcoding::io;

AVFormatContext* transcoding::io::createInputContext(
    IOReader* input, int* pret) {
  AVFormatContext* ctx = NULL;
  int ret = 0;
  *pret = 0;

  ctx = avformat_alloc_context();
  if (!ctx) {
    ret = AVERROR_NOMEM;
    goto CLEANUP;
  }

  ret = input->Open();
  if (ret) {
    goto CLEANUP;
  }
  ctx->pb = input->context;
  if (!ctx->pb) {
    ret = AVERROR_NOENT;
    goto CLEANUP;
  }

  ret = avformat_open_input(&ctx, "", NULL, NULL);
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

AVFormatContext* transcoding::io::createOutputContext(
    IOWriter* output, int* pret) {
  AVFormatContext* ctx = NULL;
  int ret = 0;
  *pret = 0;

  ctx = avformat_alloc_context();
  if (!ctx) {
    ret = AVERROR_NOMEM;
    goto CLEANUP;
  }

  ret = output->Open();
  if (ret) {
    goto CLEANUP;
  }
  ctx->pb = output->context;
  if (!ctx->pb) {
    ret = AVERROR_NOENT;
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
