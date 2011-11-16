var events = require('events');
var fs = require('fs');
var util = require('util');

var queryInfo = require('./mediainfo').queryInfo;
var Ffmpeg = require('./ffmpeg').Ffmpeg;

var Transcoder = function(ffmpegInfo, inputs) {
  this.ffmpegInfo_ = ffmpegInfo;

  this.ffmpeg_ = new Ffmpeg(ffmpegInfo);
  if (inputs) {
    this.ffmpeg_.addInputs(inputs);
  }

  this.profile_ = null;
};
util.inherits(Transcoder, events.EventEmitter);
exports.Transcoder = Transcoder;

Transcoder.prototype.setProfile = function(profile) {
  this.profile_ = profile;
};

Transcoder.prototype.attachInputStream = function(stream) {
  this.ffmpeg_.addInput(stream);
};

Transcoder.prototype.attachInputFile = function(path) {
  this.ffmpeg_.addInput(path);
};

Transcoder.prototype.writeToStream = function(stream, callback) {
  this.ffmpeg_.setOutput(stream);
  this.write_(callback);
};

Transcoder.prototype.writeToFile = function(path, callback) {
  this.ffmpeg_.setOutput(path);
  this.write_(callback);
};

Transcoder.prototype.write_ = function(callback) {
  // TODO: set options from profile

  var proc = this.ffmpeg_.exec();

  // proc.stderr.on('data', function(data) {
  //   util.puts('stderr: ' + data);
  // });

  proc.on('exit', function(code) {
    if (callback) {
      callback(null);
    }
  });
};
