var util = require('util');

var Ffmpeg = require('./ffmpeg').Ffmpeg;

function getValue(line, regex) {
  var matches = regex.exec(line);
  if (matches && matches.length) {
    return matches[1];
  }
  return undefined;
}

function parseDuration(str) {
  // HH:MM:SS.SS
  var parts = str.split(':');
  var value = 0;
  value += parseInt(parts[0]) * 3600;
  value += parseInt(parts[1]) * 60;
  value += parseInt(parts[2]);
  return value;
}

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
}

function parseResolution(str) {
  // WxH
  var parts = str.split('x');
  return {
    width: parseInt(parts[0]),
    height: parseInt(parts[1])
  };
}

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

// Input #N, mov,mp4, from 'some input':
//   Metadata:
//     [stuff]
//   Duration: 00:02:06.89, start: 0.000000, bitrate: 818 kb/s
//     Stream #0.0(und): Video: h264 (Main), yuv420p, 640x360, 686 kb/s, 29.97 tbr, 1k tbn, 59.94 tbc
//     Stream #0.1(eng): Audio: aac, 44100 Hz, stereo, s16, 131 kb/s
//     Stream #1.2(und): Subtitle: text / 0x74786574, 0 kb/s

function parseResults(lines) {
  var results = [];

  var input = null;
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
        language: getValue(line, /Stream #[0-9]\.[0-9]+\(([a-z]+)\): /)
      };
      if (stream.language == 'und') {
        stream.language = undefined;
      }
      switch (stream.type) {
        case 'video':
          stream.codec =
              getValue(line, /Video: ([a-zA-Z0-9]+)[ ,]/);
          stream.profile =
              getValue(line, /Video: [a-zA-Z0-9]+ \(([a-zA-Z0-9 ]+)\), /);
          stream.resolution = parseResolution(
              getValue(line, /([0-9]+x[0-9]+)/));
          stream.bitrate = parseBitrate(
              getValue(line, /([0-9]+ kb\/s)/));
          stream.fps = parseFloat(
              getValue(line, /([0-9]+.[0-9]+) tbr/));
          break;
        case 'audio':
          stream.codec =
              getValue(line, /Audio: ([a-zA-Z0-9]+)[ ,]/);
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

  return results;
};

exports.queryInfo = function(ffmpegInfo, inputs, callback) {
  var wasSingleInput = false;
  if (!util.isArray(inputs)) {
    wasSingleInput = true;
    inputs = [inputs];
  }

  var ffmpeg = new Ffmpeg(ffmpegInfo);
  ffmpeg.addInputs(inputs);
  var proc = ffmpeg.exec();

  // All output comes on stderr
  var allOutput = '';
  proc.stderr.on('data', function(data) {
    allOutput += data;
  });

  proc.on('exit', function (code) {
    var lines = allOutput.split('\n');

    var results = parseResults(lines);

    if (wasSingleInput) {
      results = results[0];
    }
    callback(null, results);
  });
};
