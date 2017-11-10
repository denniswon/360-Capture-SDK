/****************************************************************************************************************

Filename	:	LibRTMP.h
Content		:	Streaming video which is encoded from HW(NVIDIA/AMD) encoder parts.
Copyright	:

****************************************************************************************************************/
#pragma once
#include <string>
#include "RTMP/include/librtmp/rtmp_sys.h"
#include "RTMP/include/librtmp/rtmp.h"
#include "RTMP/include/librtmp/log.h"

#include "FBCaptureStatus.h"

using namespace std;

#define FLV_HEADER_SIZE     13
#define FLV_FOOTER_SIZE      4
#define FLV_TAG_HEADER_SIZE 11
#define FLV_TAG_FOOTER_SIZE  4

typedef struct {
  uint8_t* data;
  size_t   aloc;
} FLVTAG_T;

namespace FBCapture {
  namespace Streaming {

    static inline size_t FlvtagSize(FLVTAG_T* tag) {
      return (tag->data[1] << 16) | (tag->data[2] << 8) | tag->data[3];
    }
    static inline uint32_t FlvtagTimestamp(FLVTAG_T* tag) {
      return (tag->data[7] << 24) | (tag->data[4] << 16) | (tag->data[5] << 8) | tag->data[6];
    }
    static inline const uint8_t* FlvtagRawData(FLVTAG_T* tag) {
      return tag->data;
    }
    static inline size_t FlvtagRawSize(FLVTAG_T* tag) {
      return FlvtagSize(tag) + FLV_TAG_HEADER_SIZE + FLV_TAG_FOOTER_SIZE;
    }

    class LibRTMP {
    public:
      LibRTMP();
      virtual ~LibRTMP();

      FBCAPTURE_STATUS initialize(const string& streamUrl);
      FBCAPTURE_STATUS sendFlvPacket(const char* buf, int size) const;
      FBCAPTURE_STATUS sendFlvPacketFromFile(const string& filepath);
      FBCAPTURE_STATUS close();

    private:
      static bool initSocket();
      static void flvtagInit(FLVTAG_T* tag);
      static void flvtagFree(FLVTAG_T* tag);
      static int flvtagReserve(FLVTAG_T* tag, uint32_t size);
      static FBCAPTURE_STATUS flvReadHeader(FILE* flv, int* hasAudio, int* hasVideo);

    private:
      const string* streamUrl_;
      FLVTAG_T tag_;
      uint32_t timestamp_ = 0;
      long preFrameTime_ = 0;
      long lastFrameTime_ = 0;
      RTMP* rtmp_;
      RTMPPacket* packet_;
      bool sessionInitialized_;
    };
  }
}
