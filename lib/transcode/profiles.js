// NOTE: each profile can have multiple video or audio configurations - the
// best match based on source input will be used, or default to the first
// defined. For example, if the profile defines both aac and mp3 audio, if the
// source contains either it will be copied instead of transcoded.

var video_h264_low = {
  codec: 'h264',
  profileId: 77,
  profileLevel: 30,
  bitrate: 600000
};
var video_h264_high = {
  codec: 'h264',
  profileId: 77,
  profileLevel: 30,
  bitrate: 4500000
};

var audio_aac_low = {
  codec: 'aac',
  channels: 2,
  sampleRate: 44100,
  sampleFormat: 's16',
  bitrate: 40000
};
var audio_aac_high = {
  codec: 'aac',
  channels: 2,
  sampleRate: 44100,
  sampleFormat: 's16',
  bitrate: 128000
};

var audio_mp3_low = {
  codec: 'mp3',
  channels: 2,
  sampleRate: 44100,
  sampleFormat: 's16',
  bitrate: 40000
};
var audio_mp3_high = {
  codec: 'mp3',
  channels: 2,
  sampleRate: 44100,
  sampleFormat: 's16',
  bitrate: 128000
};

function declareProfile(key, name, options) {
  var profile = {
    name: name,
    options: options
  };
  exports[key] = profile;
}

declareProfile('APPLE_IOS', 'Apple iPhone/iPad', {
  container: 'mov',
  video: video_h264_low,
  audio: [audio_aac_low, audio_mp3_low]
});

declareProfile('APPLE_TV_2', 'Apple TV 2', {
  container: 'mov',
  video: video_h264_high,
  audio: [audio_aac_high, audio_mp3_high]
});

declareProfile('PLAYSTATION_3', 'Playstation 3', {

});

declareProfile('XBOX_360', 'Xbox 360', {

});
