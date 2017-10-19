/*
 * @file flvMuxer.cpp
 * @author Jong Hyuck Won
 * @date 2017/09/14
 */

#include "FlvPacketizer.h"
#include "Log.h"

#define FLV_HEADER_SIZE 13

namespace FBCapture {
  namespace Streaming {
    FlvPacketizer::FlvPacketizer() {
      prev_size = (uint8_t*)malloc(4);
    }

    FlvPacketizer::~FlvPacketizer() {
      if (prev_size)
        free(prev_size);
    }

    FBCAPTURE_STATUS FlvPacketizer::packetizeVideo(VideoEncodePacket* packet, uint8_t** flv_packet) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      if (!getAvcDataTag(packet->buffer, packet->length, (uint32_t)packet->timestamp, packet->isKeyframe, flv_packet))
        return FBCAPTURE_FLV_SET_AVC_DATA_FAILED;

      return status;
    }

    FBCAPTURE_STATUS FlvPacketizer::packetizeAudio(AudioEncodePacket* packet, uint8_t** flv_packet) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      if (!getAacDataTag(packet->buffer, packet->length, (uint32_t)packet->timestamp, flv_packet))
        status = FBCAPTURE_FLV_SET_AAC_DATA_FAILED;

      return status;
    }

    FBCAPTURE_STATUS FlvPacketizer::getFlvHeader(bool haveAudio, bool haveVideo, uint8_t** flv_header) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      char flv_file_header[] = "FLV\x1\x5\0\0\0\x9\0\0\0\0"; // have audio and have video

      if (haveAudio && haveVideo) {
        flv_file_header[4] = 0x05;
      } else if (haveAudio && !haveVideo) {
        flv_file_header[4] = 0x04;
      } else if (!haveAudio && haveVideo) {
        flv_file_header[4] = 0x01;
      } else {
        flv_file_header[4] = 0x00;
      }

      *flv_header = (uint8_t*)malloc(FLV_HEADER_SIZE);
      memcpy(*flv_header, flv_file_header, FLV_HEADER_SIZE);

      return status;
    }

    /*
     * @brief set flv tag to the packet data buffer.
     * @param[in] buf:
     * @param[in] buf_len: flv tag body size
     * @param[in] timestamp: flv tag timestamp
     */
    bool FlvPacketizer::setFLVTag(FLV_TAG_TYPE type, uint8_t *buf, uint32_t buf_len, uint32_t timestamp, uint8_t **tagged_data) {

      flv_tag flvtag;
      memset(&flvtag, 0, sizeof(flvtag));

      flvtag.type = type;
      ui24ToBytes(flvtag.data_size, buf_len);
      flvtag.timestamp_ex = (uint8_t)((timestamp >> 24) & 0xff);
      flvtag.timestamp[0] = (uint8_t)((timestamp >> 16) & 0xff);
      flvtag.timestamp[1] = (uint8_t)((timestamp >> 8) & 0xff);
      flvtag.timestamp[2] = (uint8_t)((timestamp) & 0xff);

      uint8_t* pbuf = (uint8_t*)malloc(sizeof(flvtag) + buf_len);
      uint32_t idx = 0;

      // flv tag
      memcpy(pbuf, &flvtag, sizeof(flvtag));
      idx += (uint32_t)sizeof(flvtag);

      // actual buffer data
      memcpy(pbuf + idx, buf, buf_len);
      idx += buf_len;

      // update prev_size (24 bit)
      ui32ToBytes(prev_size, idx);

      *tagged_data = pbuf;

      return true;
    }

    /*
     * @brief write Aac sequence header in header of audio tag data part, the first audio tag, fixed by 4 bytes
     */
    FBCAPTURE_STATUS FlvPacketizer::getAacSeqHeaderTag(int profile_level, int sample_rate, int num_channels, uint8_t** aac_seq_header_packet) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      uint8_t *buf = (uint8_t *)malloc(4);
      uint8_t *pbuf = buf;

      uint8_t flag = 0;
      // SoundFormat (UB4) 10 - AAC, SoundRate (UB2) 3 = 44-kHz, SoundSize (UB1) 1 = snd16Bit, SoundType(UB1) 1 = sndStereo
      flag = 0xaf;
      pbuf = ui08ToBytes(pbuf, flag);
      // AACPacketType: 0x00 - AAC sequence header
      pbuf = ui08ToBytes(pbuf, 0);
      // AudioObjectType (UB5) 0x2 (2: LC), samplingFrequency (UB4) 0x3 (48kHz), channelConfiguration (UB4) 0x2 (stereo)
      // first byte: (profile_level=0x2 << 3) | (sample_rate=0x3 >> 1 & 0x5)
      pbuf = ui08ToBytes(pbuf, (profile_level << 3) | (sample_rate >> 1 & 0x5));
      // second byte: (sample_rate=0x3 & 0x1) << 7 | (num_channels=2 & 0xf) << 3
      pbuf = ui08ToBytes(pbuf, (sample_rate & 0x1) << 7 | (num_channels & 0xf) << 3);

      uint8_t* flv_tag_buf;
      if (!setFLVTag(FLV_TAG_TYPE::AUDIO, buf, (uint32_t)(pbuf - buf), 0, &flv_tag_buf)) {
        DEBUG_ERROR("Failed setting FLV aac sequence header tag.");
        return FBCAPTURE_FLV_SET_AAC_SEQ_HEADER_FAILED;
      }

      *aac_seq_header_packet = flv_tag_buf;

      return status;
    }

    /*
     * @brief write header of aac audio data, fixed 2 bytes
     */
    FBCAPTURE_STATUS FlvPacketizer::getAacDataTag(const uint8_t *data, uint32_t data_len, uint32_t timestamp, uint8_t** aac_data) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      uint8_t *buf = (uint8_t *)malloc(data_len + 2);
      uint8_t *pbuf = buf;

      uint8_t flag = 0;
      // SoundFormat (UB4) 10 - AAC, SoundRate (UB2) 3 = 44-kHz, SoundSize (UB1) 1 = snd16Bit, SoundType(UB1) 1 = sndStereo
      flag = 0xaf;
      pbuf = ui08ToBytes(pbuf, flag);

      pbuf = ui08ToBytes(pbuf, 1);    // AACPacketType: 0x01 - Raw AAC frame data

      memcpy(pbuf, data, data_len);
      pbuf += data_len;

      uint8_t* flv_tag_buf;
      if (!setFLVTag(FLV_TAG_TYPE::AUDIO, buf, (uint32_t)(pbuf - buf), timestamp, &flv_tag_buf)) {
        DEBUG_ERROR("Failed setting FLV aac data tag.");
        return FBCAPTURE_FLV_SET_AAC_DATA_FAILED;
      }

      *aac_data = buf;

      return status;
    }

    /*
     * @brief write AVC sequence header in header of video tag data part, the first video tag
     */
    FBCAPTURE_STATUS FlvPacketizer::getAvcSeqHeaderTag(const uint8_t *sps, uint32_t sps_len, const uint8_t *pps, uint32_t pps_len, uint8_t** avc_seq_header_packet) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      // AVCC length 11 + 2 (sps_len) + sps_len (sps length) + 1 (number of pps) + 2 (pps_len) + pps_len (pps length)
      uint8_t* avc_seq_buf = (uint8_t *)malloc(11 + 2 + sps_len + 1 + 2 + pps_len);
      uint8_t* pbuf = avc_seq_buf;

      uint8_t flag = 0;

      flag = (1 << 4) // frametype "1 == keyframe"
        | 7; // codecid "7 == AVC"

      pbuf = ui08ToBytes(pbuf, flag);

      pbuf = ui08ToBytes(pbuf, 0); // AVCPacketType: 0x00 - AVC sequence header
      pbuf = ui24ToBytes(pbuf, 0); // composition time

                                   // generate AVCC with sps and pps, AVCDecoderConfigurationRecord

      pbuf = ui08ToBytes(pbuf, 1);      // configurationVersion
      pbuf = ui08ToBytes(pbuf, sps[1]); // AVCProfileIndication
      pbuf = ui08ToBytes(pbuf, sps[2]); // profile_compatibility
      pbuf = ui08ToBytes(pbuf, sps[3]); // AVCLevelIndication
                                        // 6 bits reserved (111111) + 2 bits nal size length - 1
                                        // (Reserved << 2) | Nal_Size_length = (0x3F << 2) | 0x03 = 0xFF
      pbuf = ui08ToBytes(pbuf, 0xff);
      // 3 bits reserved (111) + 5 bits number of sps (00001)
      // (Reserved << 5) | Number_of_SPS = (0x07 << 5) | 0x01 = 0xe1
      pbuf = ui08ToBytes(pbuf, 0xe1);

      // sps
      pbuf = ui16ToBytes(pbuf, (uint16_t)sps_len);
      memcpy(pbuf, sps, sps_len);
      pbuf += sps_len;

      // pps
      pbuf = ui08ToBytes(pbuf, 1); // number of pps
      pbuf = ui16ToBytes(pbuf, (uint16_t)pps_len);
      memcpy(pbuf, pps, pps_len);
      pbuf += pps_len;

      uint8_t* flv_tag_buf;
      if (!setFLVTag(FLV_TAG_TYPE::VIDEO, avc_seq_buf, (uint32_t)(pbuf - avc_seq_buf), 0, &flv_tag_buf)) {
        DEBUG_ERROR("Failed setting FLV avc sequence header tag.");
        return FBCAPTURE_FLV_SET_AVC_SEQ_HEADER_FAILED;
      }

      *avc_seq_header_packet = flv_tag_buf;

      return status;
    }

    /*
     * @brief write header of video tag data part, fixed 5 bytes
     */
    FBCAPTURE_STATUS FlvPacketizer::getAvcDataTag(const uint8_t *data, uint32_t data_len, uint32_t timestamp, int is_keyframe, uint8_t** avc_data_packet) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      uint8_t *buf = (uint8_t *)malloc(data_len + 5);
      uint8_t *pbuf = buf;

      uint8_t flag = 0;
      // (FrameType << 4) | CodecID, 1 - keyframe, 2 - inner frame, 7 - AVC(h264)
      if (is_keyframe) {
        flag = 0x17;
      } else {
        flag = 0x27;
      }

      pbuf = ui08ToBytes(pbuf, flag);

      pbuf = ui08ToBytes(pbuf, 1);    // AVCPacketType: 0x00 - AVC sequence header; 0x01 - AVC NALU
      pbuf = ui24ToBytes(pbuf, 0);    // composition time

      memcpy(pbuf, data, data_len);
      pbuf += data_len;

      uint8_t* flv_tag_buf;
      if (!setFLVTag(FLV_TAG_TYPE::VIDEO, buf, (uint32_t)(pbuf - buf), timestamp, &flv_tag_buf)) {
        DEBUG_ERROR("Failed setting FLV avc data tag.");
        return FBCAPTURE_FLV_SET_AVC_DATA_FAILED;
      }

      *avc_data_packet = flv_tag_buf;

      return status;
    }

    FBCAPTURE_STATUS FlvPacketizer::getPreviousTagSize(uint8_t** prev_tag_size) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;
      *prev_tag_size = prev_size;
      return status;
    }
  }
}