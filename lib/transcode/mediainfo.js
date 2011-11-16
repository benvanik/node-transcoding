var child_process = require('child_process');
var events = require('events');
var fs = require('fs');
var path = require('path');
var util = require('util');

exports.queryInfo = function(inputs, callback) {
  // Build input arguments
  // If only one is a stream, can pipe to stdin, otherwise, need to write to
  // files first
  var inputStream = null;
  var inputPaths = [];
  var wasSingleInput = false;
  if (!util.isArray(inputs)) {
    wasSingleInput = true;
    inputs = [inputs];
  }
  for (var n = 0; n < inputs.length; n++) {
    var input = inputs[n];
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
        throw 'unsupported: multiple input streams';
      }
    }
  }

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

  // Input #N, mov,mp4, from 'some input':
  //   Metadata:
  //     [stuff]
  //   Duration: 00:02:06.89, start: 0.000000, bitrate: 818 kb/s
  //     Stream #0.0(und): Video: h264 (Main), yuv420p, 640x360, 686 kb/s, 29.97 tbr, 1k tbn, 59.94 tbc
  //     Stream #0.1(eng): Audio: aac, 44100 Hz, stereo, s16, 131 kb/s
  //     Stream #1.2(und): Subtitle: text / 0x74786574, 0 kb/s

  var proc = child_process.spawn('ffmpeg', args);
  var allOutput = '';
  proc.stdout.on('data', function(data) {
    //console.log('stdout: ' + data);
  });
  proc.stderr.on('data', function(data) {
    //console.log('stderr: ' + data);
    allOutput += data;
  });
  proc.on('exit', function (code) {
    function getValue(line, regex) {
      var matches = regex.exec(line);
      if (matches && matches.length) {
        return matches[1];
      }
      return undefined;
    };

    function parseDuration(str) {
      // HH:MM:SS.SS
      var parts = str.split(':');
      var value = 0;
      value += parseInt(parts[0]) * 3600;
      value += parseInt(parts[1]) * 60;
      value += parseInt(parts[2]);
      return value;
    };

    function parseBitrate(str) {
      // N kb/s
      var parts = str.split(' ');
      var value = parseInt(parts[0]);
      switch (parts[1]) {
        case 'b/s':
          value *= 1;
          break;
        case 'kb/s':
          value *= 1000;
          break;
        case 'mb/s':
          value *= 1000000;
          break;
        default:
          throw new Error('unsupported bitrate unit ' + parts[1]);
      }
      return value;
    };

    function parseResolution(str) {
      // WxH
      var parts = str.split('x');
      return {
        width: parseInt(parts[0]),
        height: parseInt(parts[1])
      };
    };

    function parseChannels(str) {
      switch (str) {
        case 'stereo':
          return 2;
        case 'mono':
          return 1;
        default:
          return 0;
      }
    }

    var results = [];
    var input = null;
    var lines = allOutput.split('\n');
    for (var n = 0; n < lines.length; n++) {
      var line = lines[n];
      if (line.indexOf('Input #') == 0) {
        input = {
          container: getValue(line, /Input #[0-9]+, ([a-zA-Z0-9,]+),/),
          duration: 0,
          start: 0,
          bitrate: 0,
          streams: []
        };
        results.push(input);
      }
      if (line.indexOf('  Duration: ') == 0) {
        input.duration = parseDuration(
            getValue(line, /Duration: ([0-9:\.]+),/));
        input.start = parseFloat(
            getValue(line, /start: ([0-9]+.[0-9]+),/));
        input.bitrate = parseBitrate(
            getValue(line, /bitrate: ([0-9]+ kb\/s)/));
      }
      if (line.indexOf('    Stream #') == 0) {
        var stream = {
          type: getValue(line, /: (Audio|Video|Subtitle): /).toLowerCase(),
          language: getValue(line, /Stream #[0-9]\.[0-9]+\(([a-z]+)\): /),
        };
        if (stream.language == 'und') {
          stream.language = undefined;
        }
        // TODO: codec profile (like 'Video: h264 (Profile),')
        switch (stream.type) {
          case 'video':
            stream.codec = getValue(line, /Video: ([a-zA-Z0-9]+)[ ,]/);
            stream.resolution = parseResolution(
                getValue(line, /([0-9]+x[0-9]+)/));
            stream.bitrate = parseBitrate(
                getValue(line, /([0-9]+ kb\/s)/));
            stream.fps = parseFloat(
                getValue(line, /([0-9]+.[0-9]+) tbr/));
            break;
          case 'audio':
            stream.codec = getValue(line, /Audio: ([a-zA-Z0-9]+)[ ,]/);
            stream.sampleRate = parseInt(
                getValue(line, /([0-9]+) Hz, /));
            stream.channels = parseChannels(
                getValue(line, /Hz, ([a-zA-Z0-9]+), /));
            stream.bitrate = parseBitrate(
                getValue(line, /([0-9]+ kb\/s)/));
            break;
          case 'subtitle':
            break;
        }
        input.streams.push(stream);
      }
    }

    if (wasSingleInput) {
      results = results[0];
    }
    callback(null, results);
  });
};
