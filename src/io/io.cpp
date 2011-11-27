#include "io.h"

using namespace transcoding;
using namespace transcoding::io;

AVFormatContext* transcoding::io::createInputContext(
    IOReader* input, int* pret) {
  TC_LOG_D("io::createInputContext()\n");
  AVFormatContext* ctx = NULL;
  int ret = 0;
  *pret = 0;

  ctx = avformat_alloc_context();
  if (!ctx) {
    ret = AVERROR_NOMEM;
    TC_LOG_D("io::createInputContext(): failed alloc ctx (%d)\n", ret);
    goto CLEANUP;
  }

  ret = input->Open();
  if (ret) {
    TC_LOG_D("io::createInputContext(): failed open (%d)\n", ret);
    goto CLEANUP;
  }
  ctx->pb = input->context;
  if (!ctx->pb) {
    ret = AVERROR_NOENT;
    TC_LOG_D("io::createInputContext(): no pb (%d)\n", ret);
    goto CLEANUP;
  }

  ret = avformat_open_input(&ctx, "", NULL, NULL);
  if (ret < 0) {
    TC_LOG_D("io::createInputContext(): failed open_input (%d)\n", ret);
    goto CLEANUP;
  }

  // Prevent avio_close (which would hose us)
  ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

  ret = av_find_stream_info(ctx);
  if (ret < 0) {
    TC_LOG_D("io::createInputContext(): failed find_stream_info (%d)\n", ret);
    goto CLEANUP;
  }

  TC_LOG_D("io::createInputContext(): success\n");
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
  TC_LOG_D("io::createOutputContext()\n");
  AVFormatContext* ctx = NULL;
  int ret = 0;
  *pret = 0;

  ctx = avformat_alloc_context();
  if (!ctx) {
    ret = AVERROR_NOMEM;
    TC_LOG_D("io::createOutputContext(): failed alloc ctx (%d)\n", ret);
    goto CLEANUP;
  }

  ret = output->Open();
  if (ret) {
    TC_LOG_D("io::createOutputContext(): failed open (%d)\n", ret);
    goto CLEANUP;
  }
  ctx->pb = output->context;
  if (!ctx->pb) {
    ret = AVERROR_NOENT;
    TC_LOG_D("io::createOutputContext(): no pb (%d)\n", ret);
    goto CLEANUP;
  }

  TC_LOG_D("io::createOutputContext(): success\n");
  return ctx;

CLEANUP:
  if (ctx) {
    avformat_free_context(ctx);
  }
  *pret = ret;
  return NULL;
}
