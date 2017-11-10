#pragma once

#include "FBCaptureEncoderModule.h"
#include "GPUEncoder.h"

using namespace FBCapture::Streaming;

namespace FBCapture {
  namespace Video {

    class VideoEncoder : public FBCaptureEncoderModule {
    public:
      VideoEncoder(FBCaptureEncoderDelegate *mainDelegate,
                   EncodePacketProcessorDelegate *processorDelegate,
                   GRAPHICS_CARD_TYPE type,
                   ID3D11Device* device,
                   uint32_t bitrate,
                   uint32_t fps,
                   uint32_t gop,
                   bool flipTexture,
                   bool enableAsyncMode);
      ~VideoEncoder();

      FBCAPTURE_STATUS encode(void *texturePtr);

    protected:
      GPUEncoder* gpuEncoder_;

      GRAPHICS_CARD_TYPE graphicsCardType_;
      ID3D11Device* device_;

      uint32_t bitrate_;
      uint32_t fps_;
      uint32_t gop_;
      bool flipTexture_;

      /* FBCaptureEncoderModule */

      FBCAPTURE_STATUS init() override;
      FBCAPTURE_STATUS getPacket(EncodePacket** packet) override;
      FBCAPTURE_STATUS finalize() override;

      PACKET_TYPE type() override {
        return PACKET_TYPE::VIDEO;
      }

    };

  }
}