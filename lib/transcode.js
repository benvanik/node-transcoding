exports.Transcoder = require('./transcode/transcoder').Transcoder;
exports.createTranscoder = function() {
  return new exports.Transcoder();
};
