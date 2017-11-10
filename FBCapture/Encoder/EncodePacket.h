#pragma once

#include <stdint.h>
#include <stdlib.h>

using namespace std;

namespace FBCapture {
  namespace Streaming {

    using PACKET_TYPE = enum class PacketType {
      AUDIO,
      VIDEO
    };

    class EncodePacket {
    public:
      PACKET_TYPE packetType;
      uint8_t *buffer;
      uint32_t length;
      uint64_t timestamp;
      uint64_t duration;
      uint32_t frameIdx;

      EncodePacket() :
        packetType {},
        buffer(NULL),
        length { 0 },
        timestamp { 0 },
        duration { 0 },
        frameIdx { 0 } {}

      virtual ~EncodePacket() {
        free(buffer);
      }

      PACKET_TYPE type() const {
        return packetType;
      }
    };

    class VideoEncodePacket : public EncodePacket {
    public:
      bool isKeyframe;
      uint8_t *sps, *pps;
      uint32_t spsLen, ppsLen;

      VideoEncodePacket():
        isKeyframe{false},
        sps(NULL),
        pps(NULL),
        spsLen(0),
        ppsLen(0) {
        packetType = PACKET_TYPE::VIDEO;
      }

      ~VideoEncodePacket() {
        free(sps);
        free(pps);
      }
    };

    class AudioEncodePacket : public EncodePacket {
    public:
      AudioEncodePacket():
        profileLevel(0),
        sampleRate(0),
        numChannels(0) {
        packetType = PACKET_TYPE::AUDIO;
      }

      ~AudioEncodePacket() {}

      uint32_t profileLevel, sampleRate, numChannels;
    };
  }
}