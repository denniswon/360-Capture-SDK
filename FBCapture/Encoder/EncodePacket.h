#pragma once

#include <stdint.h>
#include <stdlib.h>

using namespace std;

namespace FBCapture {
  namespace Streaming {

    typedef enum class PacketType {
      VIDEO, AUDIO
    } PacketType;

    class EncodePacket {
    public:
      PacketType packetType;
      uint8_t *buffer;
      uint32_t length;
      uint64_t timestamp;
      uint64_t duration;
      uint32_t frameIdx;

      EncodePacket() {}
      virtual ~EncodePacket() {
        free(buffer);
      }

      PacketType type() {
        return packetType;
      }
    };

    class VideoEncodePacket : public EncodePacket {
    public:
      bool isKeyframe;
      uint8_t *sps, *pps;
      uint32_t spsLen, ppsLen;

      VideoEncodePacket() {
        packetType = PacketType::VIDEO;
      }
      ~VideoEncodePacket() {
        free(sps);
        free(pps);
      }
    };

    class AudioEncodePacket : public EncodePacket {
    public:
      AudioEncodePacket() {
        packetType = PacketType::AUDIO;
      }
      ~AudioEncodePacket() {}

      uint32_t profileLevel, sampleRate, numChannels;
    };
  }
}