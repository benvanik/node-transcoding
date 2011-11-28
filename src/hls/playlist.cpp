#include "playlist.h"

using namespace std;
using namespace transcoding;
using namespace transcoding::hls;

Playlist::Playlist(string& path, string& name,
    double duration, bool allowCache) :
    path(path), name(name), duration(duration) {
  TC_LOG_D("Playlist::Playlist(%s, %s, %d, %s)\n",
      path.c_str(), name.c_str(), (int)duration,
      allowCache ? "cache" : "no-cache");

  this->playlistFile = this->path + this->name + ".m3u8";

  char str[1024];
  sprintf(str,
      "#EXTM3U\n"
      "#EXT-X-VERSION:1\n"
      "#EXT-X-PLAYLIST-TYPE:EVENT\n"
      "#EXT-X-TARGETDURATION:%d\n"
      "#EXT-X-MEDIA-SEQUENCE:1\n"
      "#EXT-X-ALLOW-CACHE:%s\n",
      (int)duration, allowCache ? "YES" : "NO");
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

  uv_fs_t openReq;
  r = uv_fs_open(uv_default_loop(),
      &openReq, this->playlistFile.c_str(),
      flags, S_IWRITE | S_IREAD, NULL);

  uv_fs_t writeReq;
  r = uv_fs_write(uv_default_loop(),
      &writeReq, openReq.result,
      (void*)str, strlen(str), -1, NULL);

  uv_fs_t closeReq;
  r = uv_fs_close(uv_default_loop(),
      &closeReq, openReq.result, NULL);

  return r;
}

int Playlist::AddSegment(int id) {
  TC_LOG_D("Playlist::AddSegment(%d)\n", id);

  char str[1024];
  sprintf(str, "#EXTINF:%d,\n%s-%d.ts\n",
      (int)this->duration, this->name.c_str(), id);
  return this->AppendString(str);
}

int Playlist::Complete() {
  TC_LOG_D("Playlist::Complete()\n");

  return this->AppendString("#EXT-X-ENDLIST\n");
}
