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

  this.priority_ = 0;
  this.inputs_ = [];
  this.output_ = null;
  this.options_ = {
  };
};
exports.Ffmpeg = Ffmpeg;

Ffmpeg.prototype.setPriority = function(priority) {
  this.priority_ = priority;
  return this;
};

Ffmpeg.prototype.addInput = function(streamOrPath) {
  this.inputs_.push(streamOrPath);
  return this;
};

Ffmpeg.prototype.addInputs = function(inputs) {
  if (util.isArray(inputs)) {
    this.inputs_ = this.inputs_.concat(inputs);
  } else {
    this.inputs_.push(inputs);
  }
  return this;
};

Ffmpeg.prototype.setOutput = function(streamOrPath) {
  this.output_ = streamOrPath;
  return this;
};

Ffmpeg.prototype.setOptions = function(options) {
  this.options_ = options;
  return this;
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

  var args = [];

  // Basics
  args.push('-y'); // always overwrite output files

  // Input args
  if (inputStream) {
    args.push('-i', '-');
  }
  for (var n = 0; n < inputPaths.length; n++) {
    var inputPath = path.normalize(inputPaths[n]);
    args.push('-i', inputPath);
  }

  // Real options
  // TODO: options
  args.push('-vcodec', 'copy');
  args.push('-acodec', 'copy');

  // Output
  if (this.output_) {
    if (typeof this.output_ == 'string') {
      args.push(this.output_);
    } else {
      args.push('pipe:1');
    }
  }

  // Spawn the process and wire up input (if required)
  util.puts('calling ffmpeg:');
  util.puts('  ' + args.join(' '));
  var proc = child_process.spawn(this.info_.path, args);
  if (inputStream) {
    inputStream.resume();
    inputStream.pipe(proc.stdin);
  }

  // renice to a lower priority
  if (this.priority_ !== null) {
    child_process.exec('renice -p ' + proc.pid + ' -n ' +
        (this.priority_ ? '+' + this.priority_ : this.priority_));
  }

  return proc;
};
