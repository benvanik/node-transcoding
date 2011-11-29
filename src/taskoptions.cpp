#include "taskoptions.h"

using namespace transcoding;

LiveStreamingOptions::LiveStreamingOptions(Handle<Object> source) {
  HandleScope scope;

  this->path = V8GetString(source, "path", "/tmp/");
  this->name = V8GetString(source, "name", "video");

  this->segmentDuration = V8GetNumber(source, "segmentDuration", 10);
  this->allowCaching = V8GetBoolean(source, "allowCaching", true);
}

LiveStreamingOptions::~LiveStreamingOptions() {
}

TaskOptions::TaskOptions(Handle<Object> source) :
    liveStreaming(NULL) {
  HandleScope scope;

  Local<Object> liveStreaming =
      Local<Object>::Cast(source->Get(String::New("liveStreaming")));
  if (!liveStreaming.IsEmpty() && liveStreaming->IsObject()) {
    this->liveStreaming = new LiveStreamingOptions(liveStreaming);
  }
}

TaskOptions::~TaskOptions() {
  if (this->liveStreaming) {
    delete this->liveStreaming;
  }
}
