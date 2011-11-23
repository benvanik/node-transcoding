var util = require('util');

var binding = require('./transcode/binding');

exports.profiles = require('./transcode/profiles');

// Enable extra FFMPEG debug spew
binding.setDebugLevel(true);

exports.StreamType = {
  AUDIO: 'audio',
  VIDEO: 'video',
  SUBTITLE: 'subtitle'
};

exports.queryInfo = function(source, callback) {
  binding.queryInfo(source, callback);
};

exports.createTask = function(source, target, profile, opt_options) {
  return new binding.Task(source, target, profile, opt_options || {});
};

exports.transcode = function(source, target, profile, opt_options, callback) {
  var sourceInfoStash = null;
  var targetInfoStash = null;
  var task = new binding.Task(source, target, profile, opt_options || {});
  task.on('begin', function(sourceInfo, targetInfo) {
    sourceInfoStash = sourceInfo;
    targetInfoStash = targetInfo;
  });
  task.on('error', function(err) {
    callback(err, undefined, undefined);
  });
  task.on('end', function() {
    callback(undefined, sourceInfoStash, targetInfoStash);
  });
  task.start();
};
