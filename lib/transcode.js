var util = require('util');

var binding = require('./transcode/binding');

exports.profiles = require('./transcode/profiles');

// Enable extra FFMPEG debug spew
binding.setDebugLevel(true);

exports.queryInfo = function(source, callback) {
  binding.queryInfo(source, callback);
};

exports.createTask = function(source, target, options) {
  return new binding.Task(source, target, options);
};

exports.transcode = function(source, target, profile, callback) {
  var sourceInfoStash = null;
  var targetInfoStash = null;
  var task = exports.createTask(source, target, {
    profile: profile
  });
  task.on('begin', function(sourceInfo, targetInfo) {
    sourceInfoStash = sourceInfo;
    targetInfoStash = targetInfo;
  });
  task.on('error', function(err) {
    callback(err, sourceInfo, targetInfo);
  });
  task.on('end', function() {
    callback(null, sourceInfo, targetInfo);
  });
  task.start();
};
