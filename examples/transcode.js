#!/usr/bin/env node

var transcode = require('transcode');
var util = require('util');

var paths = process.argv.slice(2);
if (!paths.length) {
  util.puts('no input files');
  return;
}
if (paths.length < 2) {
  util.puts('no output file');
  return;
}
var outputPath = paths.pop();

var transcoder = transcode.createTranscoder(paths);
transcoder.setProfile(transcode.profiles.APPLE_TV);
transcoder.writeToFile(outputPath, function(err) {
  util.puts('complete!');
  process.exit();
});
