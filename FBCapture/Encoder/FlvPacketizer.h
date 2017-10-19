#pragma once

/*
* @file FlvPacketizer.h
* @author Jong Hyuck Won
* @date 2017/09/14
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include "EncodePacket.h"
#include "FBCaptureStatus.h"

using namespace std;

namespace FBCapture {
  namespace Streaming {

    struct flv_tag {
      uint8_t type;
      uint8_t data_size[3];
      uint8_t timestamp[3];
      uint8_t timestamp_ex;
      uint8_t streamid[3];
    };

    enum FLV_TAG_TYPE {
      AUDIO = 0x08,
      VIDEO = 0x09,
      META = 0x12,
    };

    enum AMF_DATA_TYPE {
      AMF_DATA_NUMBER = 0x00,
      AMF_DATA_BOOL = 0x01,
      AMF_DATA_STRING = 0x02,
      AMF_DATA_OBJECT = 0x03,
      AMF_DATA_NULL = 0x05,
      AMF_DATA_UNDEFINED = 0x06,
      AMF_DATA_REFERENCE = 0x07,
      AMF_DATA_MIXEDARRAY = 0x08,
      AMF_DATA_OBJECT_END = 0x09,
      AMF_DATA_ARRAY = 0x0a,
      AMF_DATA_DATE = 0x0b,
      AMF_DATA_LONG_STRING = 0x0c,
      AMF_DATA_UNSUPPORTED = 0x0d,
    };

    class FlvPacketizer {
    public:

      FlvPacketizer();
      ~FlvPacketizer();

      FBCAPTURE_STATUS packetizeVideo(VideoEncodePacket* packet, uint8_t** flv_packet);
      FBCAPTURE_STATUS packetizeAudio(AudioEncodePacket* packet, uint8_t** flv_packet);

      FBCAPTURE_STATUS getFlvHeader(bool haveAudio, bool haveVideo, uint8_t** flv_header);
      FBCAPTURE_STATUS getAacSeqHeaderTag(int profile_level, int sample_rate, int num_channels, uint8_t** aac_seq_header_packet);
      FBCAPTURE_STATUS getAacDataTag(const uint8_t *data, uint32_t data_len, uint32_t timestamp, uint8_t** aac_dat_packeta);
      FBCAPTURE_STATUS getAvcSeqHeaderTag(const uint8_t *sps, uint32_t sps_len, const uint8_t *pps, uint32_t pps_len, uint8_t** avc_seq_header_packet);
      FBCAPTURE_STATUS getAvcDataTag(const uint8_t *data, uint32_t data_len, uint32_t timestamp, int is_keyframe, uint8_t** avc_data_packet);

      FBCAPTURE_STATUS getPreviousTagSize(uint8_t** prev_tag_size);

    private:

      uint8_t* prev_size;

    public:

      bool setFLVTag(FLV_TAG_TYPE type, uint8_t *buf, uint32_t buf_len, uint32_t timestamp, uint8_t **tagged_data);

      static inline uint8_t *ui08ToBytes(uint8_t *buf, uint8_t val) {
        buf[0] = (val) & 0xff;
        return buf + 1;
      }

      static inline uint8_t *ui16ToBytes(uint8_t *buf, uint16_t val) {
        buf[0] = (val >> 8) & 0xff;
        buf[1] = (val) & 0xff;

        return buf + 2;
      }

      static inline uint8_t *ui24ToBytes(uint8_t *buf, uint32_t val) {
        buf[0] = (val >> 16) & 0xff;
        buf[1] = (val >> 8) & 0xff;
        buf[2] = (val) & 0xff;

        return buf + 3;
      }

      static inline uint8_t *ui32ToBytes(uint8_t *buf, uint32_t val) {
        buf[0] = (val >> 24) & 0xff;
        buf[1] = (val >> 16) & 0xff;
        buf[2] = (val >> 8) & 0xff;
        buf[3] = (val) & 0xff;

        return buf + 4;
      }

      static inline uint8_t *ui64ToBytes(uint8_t *buf, uint64_t val) {
        buf[0] = (val >> 56) & 0xff;
        buf[1] = (val >> 48) & 0xff;
        buf[2] = (val >> 40) & 0xff;
        buf[3] = (val >> 32) & 0xff;
        buf[4] = (val >> 24) & 0xff;
        buf[5] = (val >> 16) & 0xff;
        buf[6] = (val >> 8) & 0xff;
        buf[7] = (val) & 0xff;

        return buf + 8;
      }

      static inline uint8_t *doubleToBytes(uint8_t *buf, double val) {
        union {
          uint8_t dc[8];
          double dd;
        } d;

        uint8_t b[8];

        d.dd = val;

        b[0] = d.dc[7];
        b[1] = d.dc[6];
        b[2] = d.dc[5];
        b[3] = d.dc[4];
        b[4] = d.dc[3];
        b[5] = d.dc[2];
        b[6] = d.dc[1];
        b[7] = d.dc[0];

        memcpy(buf, b, 8);

        return buf + 8;
      }

      static inline uint8_t bytesToUi32(const uint8_t *buf) {
        return (((buf[0]) << 24) & 0xff000000)
          | (((buf[1]) << 16) & 0xff0000)
          | (((buf[2]) << 8) & 0xff00)
          | (((buf[3])) & 0xff);
      }

      static inline uint8_t *amfStringToBytes(uint8_t *buf, const char *str) {
        uint8_t *pbuf = buf;
        size_t len = strlen(str);
        pbuf = ui16ToBytes(pbuf, (uint16_t)len);
        memcpy(pbuf, str, len);
        pbuf += len;

        return pbuf;
      }

      static inline uint8_t *amf_double_to_bytes(uint8_t *buf, double d) {
        uint8_t *pbuf = buf;
        pbuf = ui08ToBytes(pbuf, AMF_DATA_NUMBER);
        pbuf = doubleToBytes(pbuf, d);

        return pbuf;
      }

      static inline uint8_t *amf_bool_to_bytes(uint8_t *buf, int b) {
        uint8_t *pbuf = buf;
        pbuf = ui08ToBytes(pbuf, AMF_DATA_BOOL);
        pbuf = ui08ToBytes(pbuf, !!b);

        return pbuf;
      }

    };
  }
}