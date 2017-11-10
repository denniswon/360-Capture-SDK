#pragma once

#include "FBCaptureModule.h"
#include "GPUEncoder.h"

namespace FBCapture {
  namespace Video {

    class ImageEncoder : public FBCaptureModule {
    public:
      ImageEncoder(FBCaptureDelegate *mainDelegate,
                   GRAPHICS_CARD_TYPE graphicsCardType,
                   ID3D11Device* device,
                   bool enableAsyncMode);
      ~ImageEncoder();

      FBCAPTURE_STATUS setInput(void *texturePtr, DESTINATION_URL dstUrl, bool flipTexture);

    protected:
      GPUEncoder* gpuEncoder_;

      ID3D11Device* device_;
      GRAPHICS_CARD_TYPE graphicsCardType_;

      void *texturePtr_;
      wchar_t* jpgFilePath_;
      bool flipTexture_;

      /* FBCaptureModule */

      FBCAPTURE_STATUS init() override;
      FBCAPTURE_STATUS process() override;
      FBCAPTURE_STATUS finalize() override;
      bool continueLoop() override;
    };
  }
}