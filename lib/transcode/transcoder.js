var events = require('events');
var fs = require('fs');
var util = require('util');

var Transcoder = function(ffmpegInfo) {
  this.ffmpegInfo_ = ffmpegInfo;
};
util.inherits(Transcoder, events.EventEmitter);
exports.Transcoder = Transcoder;

Transcoder.prototype.attachInputStream = function(stream) {
  //
};

Transcoder.prototype.attachInputFile = function(path) {
  var stream = fs.createReadStream(path);
  this.attachInputStream(stream);
};

Transcoder.prototype.writeToStream = function(stream, callback) {
  //
};

Transcoder.prototype.writeToFile = function(path, callback) {
  var stream = fs.createWriteStream(path);
  this.writeToStream(stream, callback);
};
