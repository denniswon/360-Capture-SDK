#pragma once

#include "FBCaptureEncoderModule.h"
#include "AudioCapture.h"
#include "MFAudioEncoder.h"

using namespace FBCapture::Streaming;

namespace FBCapture {
  namespace Audio {

    class AudioEncoder : public FBCaptureEncoderModule {
    public:
      AudioEncoder(FBCaptureEncoderDelegate *mainDelegate,
                   EncodePacketProcessorDelegate *processorDelegate,
                   bool mute,
                   bool mixMic,
                   bool useRiftAudioSources);
      ~AudioEncoder();

      void setOutputPath(const string* dstFile);
      void mute(bool mute);

    protected:
      AudioCapture* audioCapture_;
      MFAudioEncoder* audioEncoder_;

      EncStatus encStatus_;

      atomic<bool> mute_;                       // mute audio (controls input and output audio source together).
                                                // mute/unmute can change in the middle of an encoding session.

      bool mixMic_;                             // captures both input audio source in addition to the default output audio source
      bool useRiftAudioSources_;                // uses Rift audio input/output sources instead of the default audio devices

      const wchar_t* outputPath_;

      /* FBCaptureEncoderModule */

      FBCAPTURE_STATUS init() override;
      FBCAPTURE_STATUS getPacket(EncodePacket** packet) override;
      FBCAPTURE_STATUS finalize() override;

      PACKET_TYPE type() override {
        return PACKET_TYPE::AUDIO;
      }
    };
  }
}