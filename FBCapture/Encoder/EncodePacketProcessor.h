/****************************************************************************************************************

Filename	:	EncodePacketProcessor.h
Content		:
Copyright	:

****************************************************************************************************************/

#pragma once

#define _WINSOCKAPI_
#include <Windows.h>
#include <stdio.h>
#include <iostream>
#include <cstring>

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

      FBCAPTURE_STATUS initialize(DestinationURL dstUrl);
      const string* getOutputPath(fileExt ext);
      void finalize();
      void release();

    protected:
      FlvPacketizer* flvPacketizer;
      LibRTMP* rtmp;

      char* destinationUrl;

      string* flvOutputPath;               // flv muxing output path
      string* h264OutputPath;              // input to transmuxer for video stream
      string* aacOutputPath;               // input to transmuxer for audio stream

      FILE* h264File;
      FILE* aacFile;
      FILE* flvFile;

      bool avcSeqHdrSet;
      bool aacSeqHdrSet;

      FBCAPTURE_STATUS openOutputFiles(DestinationURL url);
      FBCAPTURE_STATUS processVideoPacket(VideoEncodePacket* packet);
      FBCAPTURE_STATUS processAudioPacket(AudioEncodePacket* packet);

      /* EncodePacketProcessorDelegate */
      FBCAPTURE_STATUS onPacket(EncodePacket* packet);
    };
  }
}