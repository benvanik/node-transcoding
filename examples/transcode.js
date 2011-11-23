#!/usr/bin/env node

var fs = require('fs');
var path = require('path');
var transcode = require('transcode');
var util = require('util');

var opts = require('tav').set({
  profile: {
    note: 'Encoding profile',
    value: 'APPLE_TV_2'
  }
});

if (!opts.args.length) {
  console.log('All profiles:');
  for (var key in transcode.profiles) {
    var profile = transcode.profiles[key];
    console.log('  ' + key + ': ' + profile.name);
  }
  return;
}

if (opts.args.length < 1) {
  console.log('no input file specified');
  return;
}

var inputFile = path.normalize(opts.args[0]);
if (!path.existsSync(inputFile)) {
  console.log('input file not found');
  return;
}

if (opts.args.length < 2) {
  // No output given, so just query info
  transcode.queryInfo(inputFile, function(err, info) {
    console.log('Info for ' + inputFile + ':');
    console.log(util.inspect(info, false, 3));
  });
  return;
}

var profile = transcode.profiles[opts['profile']];
if (!profile) {
  console.log('unknown profile: ' + profile);
  return;
}

var outputFile = path.normalize(opts.args[1]);
var outputPath = path.dirname(outputFile);
if (!path.existsSync(outputPath)) {
  fs.mkdirSync(outputPath);
}

console.log('transcoding ' + inputFile + ' -> ' + outputFile);

var task = transcode.createTask(inputFile, outputFile, profile, {
});
task.on('begin', function(sourceInfo, targetInfo) {
  // Transcoding beginning
  console.log('transcoding beginning...');
  console.log('source:');
  console.log(util.inspect(sourceInfo, false, 3));
  console.log('target:');
  console.log(util.inspect(targetInfo, false, 3));
});
task.on('progress', function(progress) {
  // New progress made, currrently at timestamp out of duration
  console.log(util.inspect(progress));
  console.log('progress ' +
      (progress.timestamp / progress.duration * 100) + '%');
});
task.on('error', function(err) {
  // Error occurred
  console.log('error: ' + err);
});
task.on('end', function() {
  // Transcoding has completed
  console.log('finished');
});
task.start();
