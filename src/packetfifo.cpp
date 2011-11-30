#include "packetfifo.h"
#include <float.h>

using namespace std;
using namespace transcoding;

StreamPacket::StreamPacket(AVPacket& packet, double timestamp) :
    packet(packet), timestamp(timestamp) {
}

StreamPacket::~StreamPacket() {
}

StreamPacketList::StreamPacketList() {
}

StreamPacketList::~StreamPacketList() {
  this->DropAllPackets();
}

int StreamPacketList::GetCount() {
  return this->packets.size();
}

void StreamPacketList::QueuePacket(AVPacket& packet, double timestamp) {
  // NOTE: assuming that packets in a stream come in order
  this->packets.push_back(new StreamPacket(packet, timestamp));
}

double StreamPacketList::GetNextTimestamp() {
  if (this->packets.size()) {
    this->packets[0]->timestamp;
  } else {
    return -1;
  }
}

bool StreamPacketList::DequeuePacket(AVPacket& packet) {
  if (!this->packets.size()) {
    return false;
  }
  StreamPacket* streamPacket = this->packets.front();
  this->packets.erase(this->packets.begin());
  packet = streamPacket->packet;
  delete streamPacket;
  return true;
}

void StreamPacketList::DropAllPackets() {
  AVPacket packet;
  while (this->DequeuePacket(packet)) {
    av_free_packet(&packet);
  }
}

PacketFifo::PacketFifo(int streamCount) :
    count(0) {
  for (int n = 0; n < streamCount; n++) {
    this->streams.push_back(new StreamPacketList());
  }
}

PacketFifo::~PacketFifo() {
  while(this->streams.size()) {
    StreamPacketList* list = this->streams.back();
    this->streams.pop_back();
    delete list;
  }
}

int PacketFifo::GetCount() {
  return this->count;
}

void PacketFifo::QueuePacket(int stream, AVPacket& packet, double timestamp) {
  this->count++;
  this->streams[stream]->QueuePacket(packet, timestamp);
}

bool PacketFifo::DequeuePacket(AVPacket& packet) {
  printf("dequeue: %d\n", this->count);
  int stream = -1;
  double lowestTimestamp = DBL_MAX;
  for (int n = 0; n < this->streams.size(); n++) {
    if (this->streams[n]->GetCount()) {
      double timestamp = this->streams[n]->GetNextTimestamp();
      if (timestamp < lowestTimestamp) {
        stream = n;
        lowestTimestamp = timestamp;
      }
    }
  }
  if (stream == -1) {
    printf("no stream\n");
    return false;
  }
  this->streams[stream]->DequeuePacket(packet);
  this->count--;
  return true;
}

void PacketFifo::DropAllPackets() {
  for (int n = 0; n < this->streams.size(); n++) {
    this->streams[n]->DropAllPackets();
  }
}
