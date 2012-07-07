#!/usr/bin/env node

var fs = require('fs');
var http = require('http');
var path = require('path');
var transcoding = require('transcoding');
var url = require('url');
var util = require('util');

var opts = require('tav').set({
  profile: {
    note: 'Encoding profile',
    value: 'APPLE_TV_2'
  },
  stream_input: {
    note: 'Stream file input to test streaming',
    value: false
  },
  stream_output: {
    note: 'Stream file output to test streaming',
    value: false
  },
  livestreaming: {
    note: 'Enable HTTP Live Streaming output',
    value: false
  },
  segmentDuration: {
    note: 'HTTP Live Streaming segment duration',
    value: 10
  }
});

if (!opts.args.length) {
  console.log('All profiles:');
  for (var key in transcoding.profiles) {
    var profile = transcoding.profiles[key];
    console.log('  ' + key + ': ' + profile.name);
  }
  return;
}

if (opts.args.length < 1) {
  console.log('no input file specified');
  return;
}

// Should really rewrite all of this flow...
// HTTP requests are 'deferred' and will call process() themselves - others
// will want it called after all this code completes
var deferredRequest = false;
var process = opts.args.length < 2 ? processQuery : processTranscode;

var inputFile = opts.args[0];
var source;
if (inputFile == '-') {
  // STDIN
  source = process.stdin;
} else if (inputFile == 'null') {
  source = null;
} else if (inputFile.indexOf('http') == 0) {
  // Web request
  var sourceUrl = url.parse(inputFile);
  var headers = {};
  headers['Accept'] = '*/*';
  headers['Accept-Charset'] = 'ISO-8859-1,utf-8;q=0.7,*;q=0.3';
  headers['Accept-Encoding'] = 'identity;q=1, *;q=0';
  headers['Accept-Language'] = 'en-US,en;q=0.8';
  headers['User-Agent'] = 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_7_2) ' +
        'AppleWebKit/535.8 (KHTML, like Gecko) Chrome/17.0.930.0 Safari/535.8';
  var req = http.request({
    hostname: sourceUrl.hostname,
    port: sourceUrl.port,
    method: 'GET',
    path: sourceUrl.path,
    headers: headers
  }, function(res) {
    source = res;
    process(source, target);
  });
  req.end();
  deferredRequest = true;
} else {
  // Local file
  inputFile = path.normalize(inputFile);
  if (!fs.existsSync(inputFile)) {
    console.log('input file not found');
    return;
  }
  source = inputFile;

  // Test files via streams
  if (opts['stream_input']) {
    source = fs.createReadStream(inputFile);
  }
}

var transcodeOptions = {
};

var target;
if (opts.args.length >= 2) {
  var profile = transcoding.profiles[opts['profile']];
  if (!profile) {
    console.log('unknown profile: ' + profile);
    return;
  }

  var outputFile = opts.args[1];
  if (opts['livestreaming']) {
    // Must be a path
    var outputPath = path.dirname(outputFile);
    if (!fs.existsSync(outputPath)) {
      fs.mkdirSync(outputPath);
    }
    target = null;
    transcodeOptions.liveStreaming = {
      path: outputPath,
      name: path.basename(outputFile),
      segmentDuration: parseInt(opts['segmentDuration']),
      allowCaching: true
    };
  } else if (outputFile == '-') {
    // STDOUT
    target = process.stdout;
  } else if (outputFile == 'null') {
    target = null;
  } else if (outputFile.indexOf('http') == 0) {
    // TODO: setup server for streaming?
    console.log('not yet implemented: HTTP serving');
    return;
  } else {
    // Local file
    outputFile = path.normalize(outputFile);
    var outputPath = path.dirname(outputFile);
    if (!fs.existsSync(outputPath)) {
      fs.mkdirSync(outputPath);
    }
    target = outputFile;

    // Test files via streams
    if (opts['stream_output']) {
      target = fs.createWriteStream(outputFile);
    }
  }

  console.log('transcoding ' + inputFile + ' -> ' + outputFile);
} else {
  console.log('querying info on ' + inputFile);
}

function processQuery(source) {
  transcoding.queryInfo(source, function(err, info) {
    console.log('Info for ' + inputFile + ':');
    console.log(util.inspect(info, false, 3));
  });
};

function processTranscode(source, target) {
  var task = transcoding.createTask(source, target, profile, transcodeOptions);
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
    //console.log(util.inspect(progress));
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
};

if (!deferredRequest) {
  process(source, target);
}
