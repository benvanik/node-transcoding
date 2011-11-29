#include "playlist.h"

using namespace std;
using namespace transcoding;
using namespace transcoding::hls;

// TODO: use a new protocol version to get floating point numbers for durations
// v3+ allow them in EXTINF

Playlist::Playlist(string& path, string& name,
    double segmentDuration, bool allowCache) :
    path(path), name(name), segmentDuration(segmentDuration) {
  TC_LOG_D("Playlist::Playlist(%s, %s, %d, %s)\n",
      path.c_str(), name.c_str(), (int)(segmentDuration + 0.5),
      allowCache ? "cache" : "no-cache");

  this->playlistFile = this->path + this->name + ".m3u8";

  char str[1024];
  sprintf(str,
      "#EXTM3U\n"
      "#EXT-X-VERSION:1\n"
      "#EXT-X-PLAYLIST-TYPE:EVENT\n"
      "#EXT-X-TARGETDURATION:%d\n"
      "#EXT-X-MEDIA-SEQUENCE:0\n"
      "#EXT-X-ALLOW-CACHE:%s\n",
      (int)(segmentDuration + 0.5), allowCache ? "YES" : "NO");
  this->AppendString(str, false);
}

Playlist::~Playlist() {
  TC_LOG_D("Playlist::~Playlist()\n");
}

string Playlist::GetSegmentPath(int id) {
  char name[64];
  sprintf(name, "-%d.ts", id);
  return this->path + this->name + name;
}

int Playlist::AppendString(const char* str, bool append) {
  int r = 0;

  int flags = O_WRONLY | O_EXLOCK;
  if (append) {
    flags |= O_APPEND;
  } else {
     flags |= O_CREAT | O_TRUNC;
  }

  bool opened = false;
  uv_fs_t openReq;
  r = uv_fs_open(uv_default_loop(),
      &openReq, this->playlistFile.c_str(),
      flags, S_IWRITE | S_IREAD, NULL);
  assert(r == 0);
  if (!r) {
    opened = true;
  }

  if (!r) {
    uv_fs_t writeReq;
    r = uv_fs_write(uv_default_loop(),
        &writeReq, openReq.result,
        (void*)str, strlen(str), -1, NULL);
    assert(r == 0);
  }

  if (!r) {
    // NOTE: don't do just an fdatasync, as servers need valid modification
    // times as well as data
    // TODO: required? close may already be enough
    uv_fs_t syncReq;
    r = uv_fs_fsync(uv_default_loop(),
        &syncReq, openReq.result, NULL);
    assert(r == 0);
  }

  if (opened) {
    uv_fs_t closeReq;
    r = uv_fs_close(uv_default_loop(),
        &closeReq, openReq.result, NULL);
    assert(r == 0);
  }

  return r;
}

int Playlist::AddSegment(int id, double duration) {
  TC_LOG_D("Playlist::AddSegment(%d)\n", id);

  char str[1024];
  sprintf(str, "#EXTINF:%d,\n%s-%d.ts\n",
      (int)(duration + 0.5), this->name.c_str(), id);
  return this->AppendString(str);
}

int Playlist::Complete() {
  TC_LOG_D("Playlist::Complete()\n");

  return this->AppendString("#EXT-X-ENDLIST\n");
}
