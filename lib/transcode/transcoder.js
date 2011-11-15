var events = require('events');
var util = require('util');

var Transcoder = function() {
};
util.inherits(Transcoder, events.EventEmitter);
exports.Transcoder = Transcoder;
