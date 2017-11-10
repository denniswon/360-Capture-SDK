/****************************************************************************************************************

Filename	:	LibRTMP.cpp
Content		:	Streaming video which is encoded from HW(NVIDIA/AMD) encoder parts.
Copyright	:

****************************************************************************************************************/
#include "LibRTMP.h"
#include "FileUtil.h"
#include "Log.h"

namespace FBCapture {
  namespace Streaming {

    LibRTMP::LibRTMP() :
      streamUrl_(NULL),
      timestamp_(false),
      preFrameTime_(false),
      lastFrameTime_(false),
      rtmp_(NULL),
      packet_(NULL),
      sessionInitialized_(false) {}

    LibRTMP::~LibRTMP() {
      close();
    }

    bool LibRTMP::initSocket() {
#ifdef WIN32
      WSADATA wsaData;
      const auto version = MAKEWORD(2, 2);
      return (WSAStartup(version, &wsaData) == 0);
#else
      return TRUE;
#endif
    }

    FBCAPTURE_STATUS LibRTMP::initialize(const string& streamUrl) {
      const auto packetSize = 1024 * 512;
      streamUrl_ = new string(streamUrl);

      rtmp_ = static_cast<RTMP*>(malloc(sizeof(RTMP)));
      RTMP_Init(rtmp_);
      rtmp_->Link.timeout = 60;

      flvtagInit(&tag_);

      DEBUG_LOG_VAR("Live streaming url: ", *streamUrl_);
      if (!RTMP_SetupURL(rtmp_, const_cast<char*>((*streamUrl_).c_str()))) {
        DEBUG_ERROR("Failed to setup RTMP with streaming url.");
        return FBCAPTURE_RTMP_INVALID_STREAM_URL;
      }

      //if unable,the AMF command would be 'play' instead of 'publish'
      RTMP_EnableWrite(rtmp_);

      if (!RTMP_Connect(rtmp_, NULL)) {
        DEBUG_ERROR("Failed to connect to RTMP");
        return FBCAPTURE_RTMP_CONNECTION_FAILED;
      }

      if (!RTMP_ConnectStream(rtmp_, 0)) {
        DEBUG_ERROR("Failed to connect stream");
        return FBCAPTURE_RTMP_CONNECTION_FAILED;
      }

      packet_ = static_cast<RTMPPacket*>(malloc(sizeof(RTMPPacket)));
      memset(packet_, 0, sizeof(RTMPPacket));
      RTMPPacket_Reset(packet_);
      RTMPPacket_Alloc(packet_, packetSize);

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS LibRTMP::sendFlvPacket(const char* buf, const int size) const {
      auto pkt = &rtmp_->m_write;
      char* enc;
      auto s2 = size;

      pkt->m_nChannel = 0x04;   /* source channel */
      pkt->m_nInfoField2 = rtmp_->m_stream_id;

      while (s2) {
        if (!pkt->m_nBytesRead) {
          if (size < 11)
            /* FLV pkt too small */
            return FBCAPTURE_RTMP_SEND_PACKET_FAILED;

          if (buf[0] == 'F' && buf[1] == 'L' && buf[2] == 'V') {
            buf += 13;
            s2 -= 13;
          }

          pkt->m_packetType = *buf++;
          pkt->m_nBodySize = AMF_DecodeInt24(buf);
          buf += 3;
          pkt->m_nTimeStamp = AMF_DecodeInt24(buf);
          buf += 3;
          pkt->m_nTimeStamp |= *buf++ << 24;
          buf += 3;
          s2 -= 11;

          if (((pkt->m_packetType == RTMP_PACKET_TYPE_AUDIO
                || pkt->m_packetType == RTMP_PACKET_TYPE_VIDEO) &&
               !pkt->m_nTimeStamp) || pkt->m_packetType == RTMP_PACKET_TYPE_INFO) {
            pkt->m_headerType = RTMP_PACKET_SIZE_LARGE;
          } else
            pkt->m_headerType = RTMP_PACKET_SIZE_MEDIUM;

          if (!RTMPPacket_Alloc(pkt, pkt->m_nBodySize)) {
            DEBUG_ERROR_VAR("failed to allocate packet", to_string(pkt->m_nBodySize));
            return FBCAPTURE_RTMP_SEND_PACKET_FAILED;
          }

          enc = pkt->m_body;
        } else
          enc = pkt->m_body + pkt->m_nBytesRead;

        int num = pkt->m_nBodySize - pkt->m_nBytesRead;

        if (num > s2)
          num = s2;

        memcpy(enc, buf, num);
        pkt->m_nBytesRead += num;
        s2 -= num;
        buf += num;

        if (pkt->m_nBytesRead == pkt->m_nBodySize) {
          const auto ret = RTMP_SendPacket(rtmp_, pkt, FALSE);
          RTMPPacket_Free(pkt);
          pkt->m_nBytesRead = 0;

          if (!ret)
            return FBCAPTURE_RTMP_SEND_PACKET_FAILED;

          buf += 4;
          s2 -= 4;

          if (s2 < 0) {
            break;
          }
        }
      }

      return FBCAPTURE_OK;
    }

    void LibRTMP::flvtagInit(FLVTAG_T* tag) {
      memset(tag, 0, sizeof(FLVTAG_T));
    }

    void LibRTMP::flvtagFree(FLVTAG_T* tag) {
      if (tag->data) {
        free(tag->data);
      }
      flvtagInit(tag);
    }

    int LibRTMP::flvtagReserve(FLVTAG_T* tag, uint32_t size) {
      size += FLV_TAG_HEADER_SIZE + FLV_TAG_FOOTER_SIZE;

      if (size > tag->aloc) {
        tag->data = static_cast<uint8_t*>(realloc(tag->data, size));
        tag->aloc = size;
      }

      return 0;
    }

    FBCAPTURE_STATUS LibRTMP::flvReadHeader(FILE* flv, int* hasAudio, int* hasVideo) {
      uint8_t h[FLV_HEADER_SIZE];

      if (FLV_HEADER_SIZE != fread(&h[0], 1, FLV_HEADER_SIZE, flv)) {
        return FBCAPTURE_RTMP_INVALID_FLV_HEADER;
      }

      if ('F' != h[0] || 'L' != h[1] || 'V' != h[2]) {
        return FBCAPTURE_RTMP_INVALID_FLV_HEADER;
      }

      (*hasAudio) = h[4] & 0x04;
      (*hasVideo) = h[4] & 0x01;

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS LibRTMP::sendFlvPacketFromFile(const string& filepath) {
      FBCAPTURE_STATUS status;

      int hasAudio, hasVideo;
      uint32_t type = 0;
      uint32_t datalength = 0;
      uint32_t streamid = 0;

      auto nextIsKey = 1;
      uint32_t preTagSize = 0;

      const auto file = fopen(filepath.c_str(), "rb");
      if (!file) {
        DEBUG_ERROR_VAR("Open File Error", filepath.c_str());
        status = FBCAPTURE_RTMP_INVALID_FLV_FILE;
        goto exit;
      }

      if ((status = flvReadHeader(file, &hasAudio, &hasVideo)) != FBCAPTURE_OK) {
        DEBUG_ERROR("The video file_ is not flv");
        goto exit;
      }

      if (!sessionInitialized_ && ((status = initialize(*streamUrl_)) != FBCAPTURE_OK)) {
        DEBUG_ERROR("Failed on initialzing RTMP");
        goto exit;
      }

      if (!RTMP_IsConnected(rtmp_)) {
        DEBUG_ERROR("RTMP_IsConnected() Error");
        status = FBCAPTURE_RTMP_DISCONNECTED;
        goto exit;
      }

      packet_->m_hasAbsTimestamp = 0;
      packet_->m_nChannel = 0x04;
      packet_->m_nInfoField2 = rtmp_->m_stream_id;
      packet_->m_nTimeStamp = 0;

      fseek(file, 9, SEEK_SET);
      fseek(file, 4, SEEK_CUR);

      DEBUG_LOG("Start to send video data to rtmp server");
      while (true) {
        if (!ReadU8(&type, file))
          break;
        if (!ReadU24(&datalength, file))
          break;
        if (!ReadTime(&timestamp_, file))
          break;
        if (!ReadU24(&streamid, file))
          break;

        if (type != 0x08 && type != 0x09) {
          fseek(file, datalength + 4, SEEK_CUR);
          continue;
        }

        RTMP_ClientPacket(rtmp_, packet_);

        if (fread(packet_->m_body, 1, datalength, file) != datalength)
          break;

        // Sending header only on the first flv packet
        if (!sessionInitialized_)
          packet_->m_headerType = RTMP_PACKET_SIZE_LARGE;

        packet_->m_nTimeStamp = timestamp_ + lastFrameTime_;
        packet_->m_packetType = type;
        packet_->m_nBodySize = datalength;
        packet_->m_hasAbsTimestamp = 0;
        preFrameTime_ = packet_->m_nTimeStamp;

        if (!RTMP_IsConnected(rtmp_)) {
          DEBUG_ERROR("RTMP is discounneded");
          status = FBCAPTURE_RTMP_DISCONNECTED;
          goto exit;
        }

        if (!RTMP_SendPacket(rtmp_, packet_, true)) {
          DEBUG_ERROR("Failed to send packet");
          status = FBCAPTURE_RTMP_SEND_PACKET_FAILED;
          goto exit;
        }

        if (!ReadU32(&preTagSize, file))
          break;

        if (!PeekU8(&type, file))
          break;
        if (type == 0x09) {
          if (fseek(file, 11, SEEK_CUR) != 0)
            break;
          if (!PeekU8(&type, file)) {
            break;
          }
          if (type == 0x17)
            nextIsKey = 1;
          else
            nextIsKey = 0;

          fseek(file, -11, SEEK_CUR);
        }
      }

      lastFrameTime_ = preFrameTime_;

      DEBUG_LOG_VAR("last frame time: ", to_string(lastFrameTime_));
      DEBUG_LOG_VAR("Sent video data to rtmp server. keyframe:", to_string(nextIsKey));

    exit:

      if (file)
        fclose(file);
      remove(filepath.c_str());

      return status;
    }

    FBCAPTURE_STATUS LibRTMP::close() {
      flvtagFree(&tag_);

      if (rtmp_) {
        RTMP_Close(rtmp_);
        RTMP_Free(rtmp_);
        rtmp_ = NULL;
      }

      if (packet_) {
        RTMPPacket_Free(packet_);
        packet_ = NULL;
      }

      if (streamUrl_)
        delete streamUrl_;
      streamUrl_ = NULL;

      sessionInitialized_ = false;
      lastFrameTime_ = 0;
      timestamp_ = 0;
      preFrameTime_ = 0;

      return FBCAPTURE_OK;
    }
  }
}
