var child_process = require('child_process');
var path = require('path');
var util = require('util');

// Attempts to find ffmpeg
exports.detect = function() {
  var info = {
    path: process.env.FFMPEG_PATH || 'ffmpeg'
  };
  return info;
};

var Ffmpeg = function(info) {
  this.info_ = info;

  this.inputs_ = [];
};
exports.Ffmpeg = Ffmpeg;

Ffmpeg.prototype.addInput = function(streamOrPath) {
  this.inputs_.push(streamOrPath);
};

Ffmpeg.prototype.addInputs = function(inputs) {
  this.inputs_ = this.inputs_.concat(inputs);
};

Ffmpeg.prototype.exec = function() {
  // Build input arguments
  // If only one is a stream, can pipe to stdin, otherwise, need to write to
  // files first
  var inputStream = null;
  var inputPaths = [];
  for (var n = 0; n < this.inputs_.length; n++) {
    var input = this.inputs_[n];
    if (typeof input == 'string') {
      // Path
      inputPaths.push(input);
    } else {
      // Stream
      if (!inputStream) {
        // First stream - can use as stdin
        inputStream = input;
      } else {
        // 2nd+ stream - scribble head to file
        // TODO: scribble to file
        throw new Error('unsupported: multiple input streams');
      }
    }
  }

  // Build args
  var args = [];
  if (inputStream) {
    args.push('-i');
    args.push('-');
  }
  for (var n = 0; n < inputPaths.length; n++) {
    var inputPath = path.normalize(inputPaths[n]);
    args.push('-i');
    args.push(inputPath);
  }

  return child_process.spawn(this.info_.path, args);
};
