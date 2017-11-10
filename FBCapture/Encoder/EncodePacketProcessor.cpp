/****************************************************************************************************************

Filename	:	EncodePacketProcessor.cpp
Content		:
Copyright	:

****************************************************************************************************************/

#include "EncodePacketProcessor.h"
#include "Common.h"
#include "Log.h"

namespace FBCapture {
  namespace Streaming {

    EncodePacketProcessor::EncodePacketProcessor() :
      flvPacketizer_(NULL),
      rtmp_(NULL),
      destinationUrl_(NULL),
      flvOutputPath_(NULL),
      mp4OutputPath_(NULL),
      h264OutputPath_(NULL),
      aacOutputPath_(NULL),
      h264File_(NULL),
      aacFile_(NULL),
      flvFile_(NULL),
      avcSeqHdrSet_(false),
      aacSeqHdrSet_(false) {}

    EncodePacketProcessor::~EncodePacketProcessor() {
      release();
    }

    FBCAPTURE_STATUS EncodePacketProcessor::initialize(const DESTINATION_URL dstUrl) {
      auto status = openOutputFiles(dstUrl);
      if (status != FBCAPTURE_OK)
        return status;

      if (rtmp_) {
        status = rtmp_->initialize(destinationUrl_);
        if (status != FBCAPTURE_OK)
          return status;

        uint8_t* flvHeader = nullptr;
        const auto haveAudio = true;
        const auto haveVideo = true;
        status = flvPacketizer_->getFlvHeader(haveAudio, haveVideo, &flvHeader);
        if (status != FBCAPTURE_OK) {
          DEBUG_ERROR("Failed getting FLV header");
          return status;
        }

        fwrite(flvHeader, FLV_HEADER_SIZE, sizeof(uint8_t), flvFile_);
        status = rtmp_->sendFlvPacket(reinterpret_cast<const char*>(flvHeader), FLV_HEADER_SIZE);
        if (status != FBCAPTURE_OK)
          return status;

        free(flvHeader);
      }

      avcSeqHdrSet_ = false;
      aacSeqHdrSet_ = false;

      return status;
    }

    FBCAPTURE_STATUS EncodePacketProcessor::onPacket(EncodePacket* packet) {
      auto status = FBCAPTURE_UNKNOWN_ENCODE_PACKET_TYPE;
      if (packet->type() == PACKET_TYPE::VIDEO)
        status = processVideoPacket(dynamic_cast<VideoEncodePacket*>(packet));
      else if (packet->type() == PACKET_TYPE::AUDIO)
        status = processAudioPacket(dynamic_cast<AudioEncodePacket*>(packet));
      return status;
    }

    FBCAPTURE_STATUS EncodePacketProcessor::processVideoPacket(VideoEncodePacket* packet) {
      auto status = FBCAPTURE_OK;

      fwrite(packet->buffer, 1, packet->length, h264File_);

      if (rtmp_) {
        if (!avcSeqHdrSet_) {
          uint8_t* avcHdrPacket;
          status = flvPacketizer_->getAvcSeqHeaderTag(packet->sps, packet->spsLen, packet->pps, packet->ppsLen, &avcHdrPacket);
          if (status != FBCAPTURE_OK)
            return status;

          fwrite(avcHdrPacket, 1, sizeof(avcHdrPacket), flvFile_);
          status = rtmp_->sendFlvPacket(reinterpret_cast<const char*>(avcHdrPacket), sizeof(avcHdrPacket));
          if (status != FBCAPTURE_OK)
            return status;
          avcSeqHdrSet_ = true;
          free(avcHdrPacket);
        }

        uint8_t* flvDataPacket;
        status = flvPacketizer_->getAvcDataTag(packet->buffer, packet->length, packet->timestamp, packet->isKeyframe, &flvDataPacket);
        if (status != FBCAPTURE_OK)
          return status;

        fwrite(flvDataPacket, 1, sizeof(flvDataPacket), flvFile_);
        rtmp_->sendFlvPacket(reinterpret_cast<const char*>(flvDataPacket), sizeof(flvDataPacket));
        if (status != FBCAPTURE_OK)
          return status;
        free(flvDataPacket);
      }

      return status;
    }

    FBCAPTURE_STATUS EncodePacketProcessor::processAudioPacket(AudioEncodePacket* packet) {
      auto status = FBCAPTURE_OK;

      if (aacFile_) // TODO: remove if check once aac archive fixed
        fwrite(packet->buffer, 1, packet->length, aacFile_);

      if (rtmp_) {
        if (!aacSeqHdrSet_) {
          uint8_t* aacHdrPacket;
          status = flvPacketizer_->getAacSeqHeaderTag(packet->profileLevel, packet->numChannels, packet->timestamp, &aacHdrPacket);
          if (status != FBCAPTURE_OK)
            return status;

          fwrite(aacHdrPacket, 1, sizeof(aacHdrPacket), flvFile_);
          rtmp_->sendFlvPacket(reinterpret_cast<const char*>(aacHdrPacket), sizeof(aacHdrPacket));
          if (status != FBCAPTURE_OK)
            return status;
          aacSeqHdrSet_ = true;
          free(aacHdrPacket);
        }

        uint8_t* flvDataPacket;
        status = flvPacketizer_->getAacDataTag(packet->buffer, packet->length, packet->timestamp, &flvDataPacket);
        if (status != FBCAPTURE_OK)
          return status;

        fwrite(flvDataPacket, 1, sizeof(flvDataPacket), flvFile_);
        rtmp_->sendFlvPacket(reinterpret_cast<const char*>(flvDataPacket), sizeof(flvDataPacket));
        if (status != FBCAPTURE_OK)
          return status;
        free(flvDataPacket);
      }

      return status;
    }

    FBCAPTURE_STATUS EncodePacketProcessor::openOutputFiles(const DESTINATION_URL url) {
      ConvertToByte(const_cast<wchar_t*>(url), &destinationUrl_);

      if (IsStreamingUrl(url)) {
        mp4OutputPath_ = new string(GetDefaultOutputPath(kMp4Ext).c_str());
        rtmp_ = new LibRTMP();
        flvPacketizer_ = new FlvPacketizer();
        flvOutputPath_ = new string(ChangeFileExt(*mp4OutputPath_, kMp4Ext, kFlvExt));
        OPEN_FILE(flvFile_, (*flvOutputPath_));
      } else
        mp4OutputPath_ = new string(destinationUrl_);

      h264OutputPath_ = new string(ChangeFileExt(*mp4OutputPath_, kMp4Ext, kH264Ext));
      OPEN_FILE(h264File_, (*h264OutputPath_));

      aacOutputPath_ = new string(ChangeFileExt(*mp4OutputPath_, kMp4Ext, kAacExt));
      // TODO: archive aac packets here instead from MFAudioEncoder
      // OPEN_FILE(aacFile_, (*aacOutputPath_));

      return FBCAPTURE_OK;
    }

    const string* EncodePacketProcessor::getOutputPath(FILE_EXT ext) const {
      if (ext.compare(kH264Ext) == 0)
        return h264OutputPath_;
      else if (ext.compare(kAacExt) == 0)
        return aacOutputPath_;
      else if (ext.compare(kFlvExt) == 0)
        return flvOutputPath_;
      else if (ext.compare(kMp4Ext) == 0)
        return mp4OutputPath_;
      else
        return nullptr;
    }

    void EncodePacketProcessor::finalize() {
      CLOSE_FILE(h264File_);
      CLOSE_FILE(aacFile_);
      CLOSE_FILE(flvFile_);
      if (rtmp_)
        rtmp_->close();
    }

    void EncodePacketProcessor::release() {
      finalize();

      REMOVE_FILE(h264OutputPath_);
      REMOVE_FILE(aacOutputPath_);
      REMOVE_FILE(flvOutputPath_);

      if (flvPacketizer_)
        delete flvPacketizer_;
      flvPacketizer_ = nullptr;

      if (rtmp_)
        delete rtmp_;
      rtmp_ = nullptr;

      if (destinationUrl_)
        delete destinationUrl_;
      destinationUrl_ = nullptr;
      if (mp4OutputPath_)
        delete mp4OutputPath_;
      mp4OutputPath_ = nullptr;

      avcSeqHdrSet_ = false;
      aacSeqHdrSet_ = false;
    }

  }
}