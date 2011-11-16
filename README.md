node-transcode -- Media transcoding and streaming for node.js
====================================

node-transcode is a library for enabling both offline and real-time media
transcoding. In addition to enabling the manipulation of the input media,
utilities are provided to ease serving of the output.

Currently supported features:

* Nothing!

Coming soon (maybe):

* Everything!

## Quickstart

    npm install transcode
    node
    > var transcoder = require('transcode').createTranscoder(
        'input.flv',
        'output.mp4', {
          preset: 'appletv'
        }).start(function(err, result) {
          // Done!
        });

## Installation

With [npm](http://npmjs.org):

    npm install transcode

From source:

    cd ~
    git clone https://benvanik@github.com/benvanik/node-transcode.git
    npm link node-transcode/

## API

### Querying Media Information

To quickly query media information (duration, codecs used, etc) use the
`queryInfo` API:

    var transcode = require('transcode');
    transcode.queryInfo('input.flv', function(err, info) {
      util.puts(info);
      // {
      //   container: 'flv',
      //   duration: 126,         // seconds
      //   start: 0,              // seconds
      //   bitrate: 818000,       // bits/sec
      //   streams: [
      //     {
      //       type: 'video',
      //       codec: 'h264',
      //       resolution: { width: 640, height: 360 },
      //       bitrate: 686000,
      //       fps: 29.97
      //     }, {
      //       type: 'audio',
      //       language: 'eng',
      //       codec: 'aac',
      //       sampleRate: 44100, // Hz
      //       channels: 2,
      //       bitrate: 131000
      //     }
      //   ]
      // }
    });


