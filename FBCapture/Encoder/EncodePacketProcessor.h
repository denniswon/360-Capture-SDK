/****************************************************************************************************************

Filename	:	EncodePacketProcessor.h
Content		:
Copyright	:

****************************************************************************************************************/

#pragma once

#include "LibRTMP.h"
#include "FBCaptureEncoderModule.h"
#include "FlvPacketizer.h"
#include "FileUtil.h"

using namespace std;

namespace FBCapture {
  namespace Streaming {

    class EncodePacketProcessor : public EncodePacketProcessorDelegate {
    public:
      EncodePacketProcessor();
      ~EncodePacketProcessor();

      FBCAPTURE_STATUS initialize(DESTINATION_URL dstUrl);
      const string* getOutputPath(FILE_EXT ext) const;
      void finalize();
      void release();

    protected:
      FlvPacketizer* flvPacketizer_;
      LibRTMP* rtmp_;

      char* destinationUrl_;

      string* flvOutputPath_;               // flv muxing output path
      string* mp4OutputPath_;               // mp4 muxing output path
      string* h264OutputPath_;              // input to transmuxer for video stream
      string* aacOutputPath_;               // input to transmuxer for audio stream

      FILE* h264File_;
      FILE* aacFile_;
      FILE* flvFile_;

      bool avcSeqHdrSet_;
      bool aacSeqHdrSet_;

      FBCAPTURE_STATUS openOutputFiles(DESTINATION_URL url);
      FBCAPTURE_STATUS processVideoPacket(VideoEncodePacket* packet);
      FBCAPTURE_STATUS processAudioPacket(AudioEncodePacket* packet);

      /* EncodePacketProcessorDelegate */
      virtual FBCAPTURE_STATUS onPacket(EncodePacket* packet) override;
    };
  }
}