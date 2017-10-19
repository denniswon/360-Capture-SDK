#pragma once

#include "FBCaptureModule.h"
#include "GPUEncoder.h"

namespace FBCapture {
  namespace Video {

    class ImageEncoder : public FBCaptureModule {
    public:
      ImageEncoder(FBCaptureDelegate *mainDelegate,
                   GraphicsCardType graphicsCardType,
                   ID3D11Device* device,
                   bool enableAsyncMode);
      ~ImageEncoder();

      FBCAPTURE_STATUS setInput(const void *texturePtr, DestinationURL dstUrl, bool flipTexture);

    protected:
      GPUEncoder* gpuEncoder;

      ID3D11Device* device_;
      GraphicsCardType graphicsCardType_;

      const void *texturePtr_;
      wchar_t* jpgFilePath_;
      bool flipTexture_;

      /* FBCaptureModule */

      FBCAPTURE_STATUS init();
      FBCAPTURE_STATUS process();
      FBCAPTURE_STATUS finalize();
      bool continueLoop() override;
    };
  }
}