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
    > var transcode = require('transcode');
    > var transcoder = transcode.createTranscoder('input.flv');
    > transcoder.setProfile(transcode.profiles.APPLE_TV);
    > transcoder.writeToFile('output.m4v', function(err, result) {
        // Done!
      });

## Installation

With [npm](http://npmjs.org):

    npm install transcode

From source:

    cd ~
    git clone https://benvanik@github.com/benvanik/node-transcode.git
    npm link node-transcode/
    
### Dependencies

node-transcode requires the command-line `ffmpeg` tool to run. Make sure
it's installed and on your path.

#### Mac OS X

The easiest way to get ffmpeg is via [MacPorts](http://macports.org).
Install it if needed and run the following from the command line:

    sudo port install ffmpeg +gpl +lame +x264 +xvid
    
#### FreeBSD

    sudo pkg_add ffmpeg

#### Linux

    sudo apt-get install ffmpeg

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


