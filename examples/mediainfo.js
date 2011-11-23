#!/usr/bin/env node

var fs = require('fs');
var path = require('path');
var transcoding = require('transcoding');
var util = require('util');

var opts = require('tav').set({
});

if (opts.args.length < 1) {
  console.log('no input file specified');
  return;
}

var inputFile = path.normalize(opts.args[0]);
if (!path.existsSync(inputFile)) {
  console.log('input file not found');
  return;
}

transcoding.queryInfo(inputFile, function(err, info) {
  console.log('Info for ' + inputFile + ':');
  if (err) {
    console.log('Error!');
    console.log(err);
  } else {
    console.log(util.inspect(info, false, 3));
  }
});
