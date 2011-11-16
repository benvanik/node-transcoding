#!/usr/bin/env node

var transcode = require('transcode');
var util = require('util');

var paths = process.argv.slice(2);
if (!paths.length) {
  util.puts('no input files');
  return;
}

transcode.queryInfo(paths, function(err, infos) {
  for (var n = 0; n < infos.length; n++) {
    var info = infos[n];
    util.puts('Info for ' + paths[n] + ':');
    util.puts(util.inspect(info, false, 3));
  }
});
