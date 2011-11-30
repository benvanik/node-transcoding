#include <node.h>
#include <v8.h>
#include <vector>
#include "utils.h"

#ifndef NODE_TRANSCODING_PACKETFIFO
#define NODE_TRANSCODING_PACKETFIFO

using namespace v8;

namespace transcoding {

class StreamPacket {
public:
  StreamPacket(AVPacket& packet, double timestamp);
  ~StreamPacket();

  double timestamp;
  AVPacket packet;
};

class StreamPacketList {
public:
  StreamPacketList();
  ~StreamPacketList();

  int GetCount();

  void QueuePacket(AVPacket& packet, double timestamp);
  double GetNextTimestamp();
  bool DequeuePacket(AVPacket& packet);
  void DropAllPackets();

private:
  std::vector<StreamPacket*> packets;
};

class PacketFifo {
public:
  PacketFifo(int streamCount);
  ~PacketFifo();

  int GetCount();

  void QueuePacket(int stream, AVPacket& packet, double timestamp);
  bool DequeuePacket(AVPacket& packet);
  void DropAllPackets();

private:
  int count;
  std::vector<StreamPacketList*> streams;
};

}; // transcoding

#endif // NODE_TRANSCODING_PACKETFIFO
