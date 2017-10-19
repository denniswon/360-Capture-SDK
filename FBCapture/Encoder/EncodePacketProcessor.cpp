/****************************************************************************************************************

Filename	:	EncodePacketProcessor.cpp
Content		:
Copyright	:

****************************************************************************************************************/

#include "EncodePacketProcessor.h"
#include "Common.h"
#include "Log.h"

#define OPEN_FILE(file, path) \
errno_t err = fopen_s(&file, path.c_str(), "wb"); \
if (err) { \
  DEBUG_ERROR_VAR("Failed opening file", path); \
  return FBCAPTURE_OUTPUT_FILE_OPEN_FAILED; \
}

#define CLOSE_FILE(file, pathPtr) \
if (file) fclose(file); \
if (pathPtr) delete pathPtr; \
file = NULL; \
pathPtr = NULL;

namespace FBCapture {
  namespace Streaming {

    EncodePacketProcessor::EncodePacketProcessor() :
      avcSeqHdrSet(false),
      aacSeqHdrSet(false),
      flvPacketizer(NULL),
      rtmp(NULL),
      h264File(NULL),
      aacFile(NULL),
      flvFile(NULL) {}

    EncodePacketProcessor::~EncodePacketProcessor() {
      release();

      if (flvPacketizer) {
        delete flvPacketizer;
        flvPacketizer = NULL;
      }

      if (rtmp) {
        delete rtmp;
        rtmp = NULL;
      }

      if (h264File) {
        delete h264File;
        h264File = NULL;
      }

      if (aacFile) {
        delete aacFile;
        aacFile = NULL;
      }

      if (flvFile) {
        delete flvFile;
        flvFile = NULL;
      }
    }

    FBCAPTURE_STATUS EncodePacketProcessor::initialize(DestinationURL dstUrl) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      status = openOutputFiles(dstUrl);
      if (status != FBCAPTURE_OK)
        return status;

      if (rtmp) {
        status = rtmp->initialize(destinationUrl);
        if (status != FBCAPTURE_OK)
          return status;

        uint8_t* flv_header = nullptr;
        bool haveAudio = true;
        bool haveVideo = true;
        status = flvPacketizer->getFlvHeader(haveAudio, haveVideo, &flv_header);
        if (status != FBCAPTURE_OK) {
          DEBUG_ERROR("Failed getting FLV header");
          return status;
        }

        fwrite(flv_header, FLV_HEADER_SIZE, sizeof(uint8_t), flvFile);
        status = rtmp->sendFlvPacket(reinterpret_cast<const char*>(flv_header), FLV_HEADER_SIZE);
        if (status != FBCAPTURE_OK)
          return status;

        free(flv_header);
      }

      avcSeqHdrSet = false;
      aacSeqHdrSet = false;

      return status;
    }

    FBCAPTURE_STATUS EncodePacketProcessor::onPacket(EncodePacket* packet) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;
      if (packet->type() == PacketType::VIDEO)
        status = processVideoPacket(dynamic_cast<VideoEncodePacket*>(packet));
      else if (packet->type() == PacketType::AUDIO)
        status = processAudioPacket(dynamic_cast<AudioEncodePacket*>(packet));
      else
        status = FBCAPTURE_UNKNOWN_ENCODE_PACKET_TYPE;
      return status;
    }

    FBCAPTURE_STATUS EncodePacketProcessor::processVideoPacket(VideoEncodePacket* packet) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      fwrite(packet->buffer, 1, packet->length, h264File);

      if (rtmp) {
        if (!avcSeqHdrSet) {
          uint8_t* avcHdrPacket;
          status = flvPacketizer->getAvcSeqHeaderTag(packet->sps, packet->spsLen, packet->pps, packet->ppsLen, &avcHdrPacket);
          if (status != FBCAPTURE_OK)
            return status;

          fwrite(avcHdrPacket, 1, sizeof(avcHdrPacket), flvFile);
          status = rtmp->sendFlvPacket(reinterpret_cast<const char*>(avcHdrPacket), sizeof(avcHdrPacket));
          if (status != FBCAPTURE_OK)
            return status;
          avcSeqHdrSet = true;
          free(avcHdrPacket);
        }

        uint8_t* flvDataPacket;
        status = flvPacketizer->getAvcDataTag(packet->buffer, packet->length, packet->timestamp, packet->isKeyframe, &flvDataPacket);
        if (status != FBCAPTURE_OK)
          return status;

        fwrite(flvDataPacket, 1, sizeof(flvDataPacket), flvFile);
        rtmp->sendFlvPacket(reinterpret_cast<const char*>(flvDataPacket), sizeof(flvDataPacket));
        if (status != FBCAPTURE_OK)
          return status;
        free(flvDataPacket);
      }

      return status;
    }

    FBCAPTURE_STATUS EncodePacketProcessor::processAudioPacket(AudioEncodePacket* packet) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      if (aacFile) // TODO: remove if check once aac archive fixed
        fwrite(packet->buffer, 1, packet->length, aacFile);

      if (rtmp) {
        if (!aacSeqHdrSet) {
          uint8_t* aacHdrPacket;
          status = flvPacketizer->getAacSeqHeaderTag(packet->profileLevel, packet->numChannels, packet->timestamp, &aacHdrPacket);
          if (status != FBCAPTURE_OK)
            return status;

          fwrite(aacHdrPacket, 1, sizeof(aacHdrPacket), flvFile);
          rtmp->sendFlvPacket(reinterpret_cast<const char*>(aacHdrPacket), sizeof(aacHdrPacket));
          if (status != FBCAPTURE_OK)
            return status;
          aacSeqHdrSet = true;
          free(aacHdrPacket);
        }

        uint8_t* flvDataPacket;
        status = flvPacketizer->getAacDataTag(packet->buffer, packet->length, packet->timestamp, &flvDataPacket);
        if (status != FBCAPTURE_OK)
          return status;

        fwrite(flvDataPacket, 1, sizeof(flvDataPacket), flvFile);
        rtmp->sendFlvPacket(reinterpret_cast<const char*>(flvDataPacket), sizeof(flvDataPacket));
        if (status != FBCAPTURE_OK)
          return status;
        free(flvDataPacket);
      }

      return status;
    }

    FBCAPTURE_STATUS EncodePacketProcessor::openOutputFiles(DestinationURL url) {
      ConvertToByte((wchar_t*)url, &destinationUrl);

      string mp4OutputPath;
      if (IsStreamingUrl(url)) {
        mp4OutputPath = GetDefaultOutputPath(kMp4Ext).c_str();
        rtmp = new LibRTMP();
        flvPacketizer = new FlvPacketizer();
        flvOutputPath = new string(ChangeFileExt(mp4OutputPath, kMp4Ext, kFlvExt));
        OPEN_FILE(flvFile, (*flvOutputPath));
      } else
        mp4OutputPath = destinationUrl;

      h264OutputPath = new string(ChangeFileExt(mp4OutputPath, kMp4Ext, kH264Ext));
      OPEN_FILE(h264File, (*h264OutputPath));

      aacOutputPath = new string(ChangeFileExt(mp4OutputPath, kMp4Ext, kAacExt));
      // TODO: archive aac packets here instead from MFAudioEncoder
      // OPEN_FILE(aacFile, (*aacOutputPath));

      return FBCAPTURE_OK;
    }

    const string* EncodePacketProcessor::getOutputPath(fileExt ext) {
      if (ext.compare(kH264Ext) == 0)
        return h264OutputPath;
      else if (ext.compare(kAacExt) == 0)
        return aacOutputPath;
      else if (ext.compare(kFlvExt) == 0)
        return flvOutputPath;
      return NULL;
    }

    void EncodePacketProcessor::finalize() {
      CLOSE_FILE(h264File, h264OutputPath);
      CLOSE_FILE(aacFile, aacOutputPath);
      CLOSE_FILE(flvFile, flvOutputPath);

      if (rtmp) {
        rtmp->close();
        rtmp = NULL;
      }

      if (destinationUrl)
        delete destinationUrl;
      destinationUrl = NULL;

      avcSeqHdrSet = false;
      aacSeqHdrSet = false;
    }

    void EncodePacketProcessor::release() {
      finalize();

      remove((*h264OutputPath).c_str());
      remove((*aacOutputPath).c_str());
      remove((*flvOutputPath).c_str());
    }
  }
}