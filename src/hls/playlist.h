#include <node.h>
#include <v8.h>
#include "../utils.h"

#ifndef NODE_TRANSCODING_HLS_PLAYLIST
#define NODE_TRANSCODING_HLS_PLAYLIST

using namespace v8;

namespace transcoding {
namespace hls {

class Playlist {
public:
  Playlist(std::string& path, std::string& name, double segmentDuration,
      bool allowCache);
  ~Playlist();

  std::string GetSegmentPath(int id);

  int AddSegment(int id, double duration);
  int Complete();

private:
  int AppendString(const char* str, bool append = true);

private:
  std::string       path;
  std::string       name;
  std::string       playlistFile;

  double            segmentDuration;
};

}; // hls
}; // transcoding

#endif // NODE_TRANSCODING_HLS_PLAYLIST
