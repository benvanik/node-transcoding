var ffmpeg = require('./transcode/ffmpeg');

var ffmpegInfo = ffmpeg.detect();

exports.verifyConfiguration = function(callback) {
  var err = null;
  if (!ffmpegInfo) {
    err = new Error('FFMPEG not found');
  }
  if (!err) {
    // TODO: chain into ffmpeg to detect features/etc
    callback(null);
  } else {
    callback(err);
  }
};

exports.Transcoder = require('./transcode/transcoder').Transcoder;
exports.createTranscoder = function() {
  return new exports.Transcoder(ffmpegInfo);
};

var queryInfo = require('./transcode/mediainfo').queryInfo;
exports.queryInfo = function(inputs, callback) {
  return queryInfo(ffmpegInfo, inputs, callback);
};
