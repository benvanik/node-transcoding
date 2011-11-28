#include "taskoptions.h"

using namespace transcoding;

StreamingOptions::StreamingOptions(Handle<Object> source) {
  HandleScope scope;

  this->segmentDuration = V8GetNumber(source, "segmentDuration", 10);
  this->allowCaching = V8GetBoolean(source, "allowCaching", true);
}

StreamingOptions::~StreamingOptions() {
}

TaskOptions::TaskOptions(Handle<Object> source) :
    streaming(NULL) {
  HandleScope scope;

  Local<Object> streaming =
      Local<Object>::Cast(source->Get(String::New("streaming")));
  if (!streaming.IsEmpty()) {
    this->streaming = new StreamingOptions(streaming);
  }
}

TaskOptions::~TaskOptions() {
  if (this->streaming) {
    delete this->streaming;
  }
}
