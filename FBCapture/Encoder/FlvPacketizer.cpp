/*
 * @file_ flvMuxer.cpp
 * @author Jong Hyuck Won
 * @date 2017/09/14
 */

#include "FlvPacketizer.h"
#include "Log.h"

#define FLV_HEADER_SIZE 13

namespace FBCapture {
  namespace Streaming {
    FlvPacketizer::FlvPacketizer() {
      prevSize_ = static_cast<uint8_t*>(malloc(4));
    }

    FlvPacketizer::~FlvPacketizer() {
      if (prevSize_)
        free(prevSize_);
    }

    FBCAPTURE_STATUS FlvPacketizer::packetizeVideo(VideoEncodePacket* packet, uint8_t** flvPacket) const {
      if (!getAvcDataTag(packet->buffer, packet->length, static_cast<uint32_t>(packet->timestamp), packet->isKeyframe, flvPacket))
        return FBCAPTURE_FLV_SET_AVC_DATA_FAILED;
      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS FlvPacketizer::packetizeAudio(AudioEncodePacket* packet, uint8_t** flvPacket) const {
      if (!getAacDataTag(packet->buffer, packet->length, static_cast<uint32_t>(packet->timestamp), flvPacket))
        return FBCAPTURE_FLV_SET_AAC_DATA_FAILED;
      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS FlvPacketizer::getFlvHeader(const bool haveAudio, const bool haveVideo, uint8_t** flv_header) {
      char flvFileHeader[] = "FLV\x1\x5\0\0\0\x9\0\0\0\0"; // have audio and have video

      if (haveAudio && haveVideo) {
        flvFileHeader[4] = 0x05;
      } else if (haveAudio && !haveVideo) {
        flvFileHeader[4] = 0x04;
      } else if (!haveAudio && haveVideo) {
        flvFileHeader[4] = 0x01;
      } else {
        flvFileHeader[4] = 0x00;
      }

      *flv_header = static_cast<uint8_t*>(malloc(FLV_HEADER_SIZE));
      memcpy(*flv_header, flvFileHeader, FLV_HEADER_SIZE);

      return FBCAPTURE_OK;
    }

    /*
     * @brief set flv tag to the packet data buffer.
     * @param[in] buf:
     * @param[in] buf_len: flv tag body size
     * @param[in] timestamp: flv tag timestamp
     */
    bool FlvPacketizer::setFlvTag(const FLV_TAG_TYPE type,
                                  uint8_t *buf,
                                  const uint32_t bufLen,
                                  const uint32_t timestamp,
                                  uint8_t **taggedData) const {

      flv_tag flvtag;
      memset(&flvtag, 0, sizeof(flvtag));

      flvtag.type = type;
      ui24ToBytes(flvtag.data_size, bufLen);
      flvtag.timestamp_ex = static_cast<uint8_t>((timestamp >> 24) & 0xff);
      flvtag.timestamp[0] = static_cast<uint8_t>((timestamp >> 16) & 0xff);
      flvtag.timestamp[1] = static_cast<uint8_t>((timestamp >> 8) & 0xff);
      flvtag.timestamp[2] = static_cast<uint8_t>((timestamp) & 0xff);

      const auto pbuf = static_cast<uint8_t*>(malloc(sizeof(flvtag) + bufLen));
      uint32_t idx = 0;

      // flv tag
      memcpy(pbuf, &flvtag, sizeof(flvtag));
      idx += static_cast<uint32_t>(sizeof(flvtag));

      // actual buffer data
      memcpy(pbuf + idx, buf, bufLen);
      idx += bufLen;

      // update prev_size (24 bit)
      ui32ToBytes(prevSize_, idx);

      *taggedData = pbuf;

      return true;
    }

    /*
     * @brief write Aac sequence header in header of audio tag data part, the first audio tag, fixed by 4 bytes
     */
    FBCAPTURE_STATUS FlvPacketizer::getAacSeqHeaderTag(const int profileLevel,
                                                       const int sampleRate,
                                                       const int numChannels,
                                                       uint8_t** aacSeqHeaderPacket) const {
      const auto buf = static_cast<uint8_t *>(malloc(4));
      auto pbuf = buf;

      // SoundFormat (UB4) 10 - AAC, SoundRate (UB2) 3 = 44-kHz, SoundSize (UB1) 1 = snd16Bit, SoundType(UB1) 1 = sndStereo
      const uint8_t flag = 0xaf;
      pbuf = ui08ToBytes(pbuf, flag);
      // AACPacketType: 0x00 - AAC sequence header
      pbuf = ui08ToBytes(pbuf, 0);
      // AudioObjectType (UB5) 0x2 (2: LC), samplingFrequency (UB4) 0x3 (48kHz), channelConfiguration (UB4) 0x2 (stereo)
      // first byte: (profile__level=0x2 << 3) | (sample_rate=0x3 >> 1 & 0x5)
      pbuf = ui08ToBytes(pbuf, (profileLevel << 3) | (sampleRate >> 1 & 0x5));
      // second byte: (sample_rate=0x3 & 0x1) << 7 | (num_channels=2 & 0xf) << 3
      pbuf = ui08ToBytes(pbuf, (sampleRate & 0x1) << 7 | (numChannels & 0xf) << 3);

      uint8_t* flvTagBuf;
      if (!setFlvTag(FLV_TAG_TYPE::AUDIO, buf, static_cast<uint32_t>(pbuf - buf), 0, &flvTagBuf)) {
        DEBUG_ERROR("Failed setting FLV aac sequence header tag.");
        return FBCAPTURE_FLV_SET_AAC_SEQ_HEADER_FAILED;
      }

      *aacSeqHeaderPacket = flvTagBuf;

      return FBCAPTURE_OK;
    }

    /*
     * @brief write header of aac audio data, fixed 2 bytes
     */
    FBCAPTURE_STATUS FlvPacketizer::getAacDataTag(const uint8_t *data,
                                                  const uint32_t dataLen,
                                                  const uint32_t timestamp,
                                                  uint8_t** aacData) const {
      const auto buf = static_cast<uint8_t *>(malloc(dataLen + 2));
      auto pbuf = buf;

      // SoundFormat (UB4) 10 - AAC, SoundRate (UB2) 3 = 44-kHz, SoundSize (UB1) 1 = snd16Bit, SoundType(UB1) 1 = sndStereo
      const uint8_t flag = 0xaf;
      pbuf = ui08ToBytes(pbuf, flag);

      pbuf = ui08ToBytes(pbuf, 1);    // AACPacketType: 0x01 - Raw AAC frame data

      memcpy(pbuf, data, dataLen);
      pbuf += dataLen;

      uint8_t* flvTagBuf;
      if (!setFlvTag(FLV_TAG_TYPE::AUDIO, buf, static_cast<uint32_t>(pbuf - buf), timestamp, &flvTagBuf)) {
        DEBUG_ERROR("Failed setting FLV aac data tag.");
        return FBCAPTURE_FLV_SET_AAC_DATA_FAILED;
      }

      *aacData = buf;

      return FBCAPTURE_OK;
    }

    /*
     * @brief write AVC sequence header in header of video tag data part, the first video tag
     */
    FBCAPTURE_STATUS FlvPacketizer::getAvcSeqHeaderTag(const uint8_t *sps,
                                                       const uint32_t spsLen,
                                                       const uint8_t *pps,
                                                       const uint32_t ppsLen,
                                                       uint8_t** avcSeqHeaderPacket) const {
      // AVCC length 11 + 2 (sps_len) + sps_len (sps length) + 1 (number of pps) + 2 (pps_len) + pps_len (pps length)
      const auto avcSeqBuf = static_cast<uint8_t *>(malloc(11 + 2 + spsLen + 1 + 2 + ppsLen));
      auto pbuf = avcSeqBuf;

      const uint8_t flag = (1 << 4) // frametype "1 == keyframe"
        | 7; // codecid "7 == AVC"

      pbuf = ui08ToBytes(pbuf, flag);

      pbuf = ui08ToBytes(pbuf, 0); // AVCPacketType: 0x00 - AVC sequence header
      pbuf = ui24ToBytes(pbuf, 0); // composition time

                                   // generate AVCC with sps and pps, AVCDecoderConfigurationRecord

      pbuf = ui08ToBytes(pbuf, 1);      // configurationVersion
      pbuf = ui08ToBytes(pbuf, sps[1]); // AVCProfile_Indication
      pbuf = ui08ToBytes(pbuf, sps[2]); // profile__compatibility
      pbuf = ui08ToBytes(pbuf, sps[3]); // AVCLevelIndication
                                        // 6 bits reserved (111111) + 2 bits nal size length - 1
                                        // (Reserved << 2) | Nal_Size_length = (0x3F << 2) | 0x03 = 0xFF
      pbuf = ui08ToBytes(pbuf, 0xff);
      // 3 bits reserved (111) + 5 bits number of sps (00001)
      // (Reserved << 5) | Number_of_SPS = (0x07 << 5) | 0x01 = 0xe1
      pbuf = ui08ToBytes(pbuf, 0xe1);

      // sps
      pbuf = ui16ToBytes(pbuf, static_cast<uint16_t>(spsLen));
      memcpy(pbuf, sps, spsLen);
      pbuf += spsLen;

      // pps
      pbuf = ui08ToBytes(pbuf, 1); // number of pps
      pbuf = ui16ToBytes(pbuf, static_cast<uint16_t>(ppsLen));
      memcpy(pbuf, pps, ppsLen);
      pbuf += ppsLen;

      uint8_t* flvTagBuf;
      if (!setFlvTag(FLV_TAG_TYPE::VIDEO, avcSeqBuf, static_cast<uint32_t>(pbuf - avcSeqBuf), 0, &flvTagBuf)) {
        DEBUG_ERROR("Failed setting FLV avc sequence header tag.");
        return FBCAPTURE_FLV_SET_AVC_SEQ_HEADER_FAILED;
      }

      *avcSeqHeaderPacket = flvTagBuf;

      return FBCAPTURE_OK;
    }

    /*
     * @brief write header of video tag data part, fixed 5 bytes
     */
    FBCAPTURE_STATUS FlvPacketizer::getAvcDataTag(const uint8_t *data,
                                                  const uint32_t dataLen,
                                                  const uint32_t timestamp,
                                                  const int isKeyframe,
                                                  uint8_t** avcDataPacket) const {
      const auto buf = static_cast<uint8_t *>(malloc(dataLen + 5));
      auto pbuf = buf;

      uint8_t flag;
      // (FrameType << 4) | CodecID, 1 - keyframe, 2 - inner frame, 7 - AVC(h264)
      if (isKeyframe) {
        flag = 0x17;
      } else {
        flag = 0x27;
      }

      pbuf = ui08ToBytes(pbuf, flag);

      pbuf = ui08ToBytes(pbuf, 1);    // AVCPacketType: 0x00 - AVC sequence header; 0x01 - AVC NALU
      pbuf = ui24ToBytes(pbuf, 0);    // composition time

      memcpy(pbuf, data, dataLen);
      pbuf += dataLen;

      uint8_t* flvTagBuf;
      if (!setFlvTag(FLV_TAG_TYPE::VIDEO, buf, static_cast<uint32_t>(pbuf - buf), timestamp, &flvTagBuf)) {
        DEBUG_ERROR("Failed setting FLV avc data tag.");
        return FBCAPTURE_FLV_SET_AVC_DATA_FAILED;
      }

      *avcDataPacket = flvTagBuf;

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS FlvPacketizer::getPreviousTagSize(uint8_t** prevTagSize) const {
      *prevTagSize = prevSize_;
      return FBCAPTURE_OK;
    }
  }
}