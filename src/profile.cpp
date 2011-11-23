#include "profile.h"

using namespace transcoding;
using namespace v8;

CodecOptions::CodecOptions(Handle<Object> source) :
    codec(""), profileId(FF_PROFILE_UNKNOWN), profileLevel(FF_LEVEL_UNKNOWN),
    bitrate(0) {
  HandleScope scope;

  this->codec = V8GetString(source, "codec", this->codec);
  this->profileId = V8GetNumber(source, "profileId", this->profileId);
  this->profileLevel = V8GetNumber(source, "profileLevel", this->profileLevel);
  this->bitrate = V8GetNumber(source, "bitrate", this->bitrate);
}

CodecOptions::~CodecOptions() {
}

AudioCodecOptions::AudioCodecOptions(Handle<Object> source) :
    CodecOptions(source), channels(2), sampleRate(44100), sampleFormat("s16") {
  HandleScope scope;

  this->channels = V8GetNumber(source, "channels", this->channels);
  this->sampleRate = V8GetNumber(source, "sampleRate", this->sampleRate);
  this->sampleFormat = V8GetString(source, "sampleFormat", this->sampleFormat);
}

AudioCodecOptions::~AudioCodecOptions() {
}

VideoCodecOptions::VideoCodecOptions(Handle<Object> source) :
    CodecOptions(source) {
  HandleScope scope;
}

VideoCodecOptions::~VideoCodecOptions() {

}

Profile::Profile(Handle<Object> source) :
    name("unknown"), container("mpegts") {
  HandleScope scope;

  this->name = V8GetString(source, "name", this->name);

  Local<Object> options =
      Local<Object>::Cast(source->Get(String::New("options")));

  this->container = V8GetString(options, "container", this->container);

  Local<Object> audio = Local<Object>::Cast(options->Get(String::New("audio")));
  Local<Object> video = Local<Object>::Cast(options->Get(String::New("video")));
  if (!audio.IsEmpty()) {
    if (audio->IsArray()) {
      Local<Array> audios = Local<Array>::Cast(audio);
      for (uint32_t n = 0; n < audios->Length(); n++) {
        this->audioCodecs.push_back(new AudioCodecOptions(
            Local<Object>::Cast(audios->Get(n))));
      }
    } else {
      this->audioCodecs.push_back(new AudioCodecOptions(audio));
    }
  }
  if (!video.IsEmpty()) {
    if (video->IsArray()) {
      Local<Array> videos = Local<Array>::Cast(video);
      for (uint32_t n = 0; n < videos->Length(); n++) {
        this->videoCodecs.push_back(new VideoCodecOptions(
            Local<Object>::Cast(videos->Get(n))));
      }
    } else {
      this->videoCodecs.push_back(new VideoCodecOptions(video));
    }
  }
}

Profile::~Profile() {
  while (!this->audioCodecs.empty()) {
    delete this->audioCodecs.back();
    this->audioCodecs.pop_back();
  }
  while (!this->videoCodecs.empty()) {
    delete this->videoCodecs.back();
    this->videoCodecs.pop_back();
  }
}
