## Remuxing

* Figure out why audio often gets garbled, video playback rates differ, etc

## Transcoding

* Codec management (decoder/encoder/etc)
* Downscaling/etc filter options (->720p, etc)

## HTTP Live Streaming

Support HTTP Live Streaming for live transcoding output to iOS/etc devices.
Should generate the main playlist (and keep it updated) as well as manage all
of the segment generation/writing/etc.

* HLSWriter IOHandle type for streaming info (manifest file, path, etc)
* Rework TaskContext to either support segmenting or subclass it
