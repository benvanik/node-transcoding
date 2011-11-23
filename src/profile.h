#include <node.h>
#include <v8.h>
#include <string>
#include <vector>
#include "utils.h"

#ifndef NODE_TRANSCODING_PROFILE
#define NODE_TRANSCODING_PROFILE

using namespace v8;

namespace transcoding {

class CodecOptions {
public:
  CodecOptions(Handle<Object> source);
  virtual ~CodecOptions();

  std::string   codec;
  int           profileId;
  int           profileLevel;
  int           bitrate;
};

class AudioCodecOptions : public CodecOptions {
public:
  AudioCodecOptions(Handle<Object> source);
  virtual ~AudioCodecOptions();

  int           channels;
  int           sampleRate;
  std::string   sampleFormat;
};

class VideoCodecOptions : public CodecOptions {
public:
  VideoCodecOptions(Handle<Object> source);
  virtual ~VideoCodecOptions();
};

class Profile {
public:
  Profile(Handle<Object> source);
  ~Profile();

  std::string                     name;

  std::string                     container;

  std::vector<AudioCodecOptions*> audioCodecs;
  std::vector<VideoCodecOptions*> videoCodecs;
};

}; // transcoding

#endif // NODE_TRANSCODING_PROFILE
