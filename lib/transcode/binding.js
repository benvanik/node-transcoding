module.exports = require('../../build/Release/node_transcode.node');

// WEAK: required in v0.6
function inheritEventEmitter(type) {
  function extend(target, source) {
    for (var k in source.prototype) {
      target.prototype[k] = source.prototype[k];
    }
  }
  var events = require('events');
  extend(type, events.EventEmitter);
}
//inheritEventEmitter(module.exports.Task);
