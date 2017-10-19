/****************************************************************************************************************

Filename	:	LibRTMP.h
Content		:	Streaming video which is encoded from HW(NVIDIA/AMD) encoder parts.
Copyright	:

****************************************************************************************************************/
#pragma once
#include <math.h>
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

#define HTON16(x)  ((x>>8&0xff)|(x<<8&0xff00))
#define HTON24(x)  ((x>>16&0xff)|(x<<16&0xff0000)|(x&0xff00))
#define HTONTIME(x) ((x>>16&0xff)|(x<<16&0xff0000)|(x&0xff00)|(x&0xff000000))
#define HTON32(x)  ((x>>24&0xff)|(x>>8&0xff00)|\
	(x<<8&0xff0000)|(x<<24&0xff000000))

typedef struct {
  uint8_t* data;
  size_t   aloc;
} flvtag_t;

namespace FBCapture {
  namespace Streaming {

    static inline size_t   flvtag_size(flvtag_t* tag) {
      return (tag->data[1] << 16) | (tag->data[2] << 8) | tag->data[3];
    }
    static inline uint32_t flvtag_timestamp(flvtag_t* tag) {
      return (tag->data[7] << 24) | (tag->data[4] << 16) | (tag->data[5] << 8) | tag->data[6];
    }
    static inline const uint8_t* flvtag_raw_data(flvtag_t* tag) {
      return tag->data;
    }
    static inline const size_t flvtag_raw_size(flvtag_t* tag) {
      return flvtag_size(tag) + FLV_TAG_HEADER_SIZE + FLV_TAG_FOOTER_SIZE;
    }

    class LibRTMP {
    public:
      LibRTMP();
      virtual ~LibRTMP();

      FBCAPTURE_STATUS initialize(const string& streamUrl);
      FBCAPTURE_STATUS sendFlvPacket(const char* buf, int size);
      FBCAPTURE_STATUS sendFlvPacketFromFile(const string& file);
      FBCAPTURE_STATUS close();

    private:
      bool initSocket();
      void flvtagInit(flvtag_t* tag);
      void flvtagFree(flvtag_t* tag);
      int flvtagReserve(flvtag_t* tag, uint32_t size);
      FBCAPTURE_STATUS flvReadHeader(FILE* flv, int* has_audio, int* has_video);
      int ReadU8(uint32_t *u8, FILE* fp);  // Read 1 uint8_t
      int ReadU16(uint32_t *u8, FILE* fp);  // Read 2 uint8_t
      int ReadU24(uint32_t *u8, FILE* fp);  // Read 3 uint8_t
      int ReadU32(uint32_t *u32, FILE*fp);  // Read 4 uint8_t
      int PeekU8(uint32_t *u8, FILE*fp);  // Read 1 uint8_t and loopback 1 uint8_t
      int ReadTime(uint32_t *utime, FILE*fp);  // Read 4 bytes and convert to time format

    private:
      const string* streamUrl_;
      flvtag_t tag_;
      uint32_t timestamp_ = 0;
      long preFrameTime_ = 0;
      long lastFrameTime_ = 0;
      RTMP* rtmp_;
      RTMPPacket* packet_;
      bool sessionInitialized;
    };
  }
}
