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
                   GraphicsCardType type,
                   ID3D11Device* device,
                   uint32_t bitrate,
                   uint32_t fps,
                   uint32_t gop,
                   bool flipTexture,
                   bool enableAsyncMode);
      ~VideoEncoder();

      FBCAPTURE_STATUS encode(const void *texturePtr);

    protected:
      GPUEncoder* gpuEncoder_;

      GraphicsCardType graphicsCardType_;
      ID3D11Device* device_;

      uint32_t bitrate_;
      uint32_t fps_;
      uint32_t gop_;
      bool flipTexture_;

      /* FBCaptureEncoderModule */

      FBCAPTURE_STATUS init();
      FBCAPTURE_STATUS getPacket(EncodePacket** packet);
      FBCAPTURE_STATUS finalize();

      const PacketType type() {
        return PacketType::VIDEO;
      }

    };

  }
}