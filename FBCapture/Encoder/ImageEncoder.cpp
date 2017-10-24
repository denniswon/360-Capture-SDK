/****************************************************************************************************************

Filename	:	ImageEncoder.cpp
Content		:
Copyright	:

****************************************************************************************************************/

#include "ImageEncoder.h"
#include "Common.h"
#include "Log.h"

namespace FBCapture {
  namespace Video {

    ImageEncoder::ImageEncoder(FBCaptureDelegate *mainDelegate,
                               GraphicsCardType graphicsCardType,
                               ID3D11Device* device,
                               bool enableAsyncMode)
      : FBCaptureModule(mainDelegate),
      device_(device),
      graphicsCardType_(graphicsCardType),
      flipTexture_(false),
      jpgFilePath_(NULL) {
      enableAsyncMode_ = enableAsyncMode;
    }

    ImageEncoder::~ImageEncoder() {
      finalize();
      if (gpuEncoder)
        GPUEncoder::deleteInstance(&gpuEncoder);
      jpgFilePath_ = NULL;
    }

    FBCAPTURE_STATUS ImageEncoder::init() {
      gpuEncoder = GPUEncoder::getInstance(graphicsCardType_, device_);
      if (!gpuEncoder) {
        DEBUG_ERROR("Unsupported graphics card. The SDK supports only nVidia and AMD GPUs");
        return FBCAPTURE_GPU_ENCODER_UNSUPPORTED_DRIVER;
      };
      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS ImageEncoder::setInput(const void *texturePtr, DestinationURL dstUrl, bool flipTexture) {
      if (!texturePtr) {
        DEBUG_ERROR("It's invalid texture pointer: null");
        return FBCAPTURE_GPU_ENCODER_NULL_TEXTURE_POINTER;
      }

      texturePtr_ = texturePtr;
      if (dstUrl) {
        jpgFilePath_ = new wchar_t[MAX_FILENAME_LENGTH];
        wcscpy(jpgFilePath_, dstUrl);
      } else
        ConvertToWide((char*)GetDefaultOutputPath(kJpgExt).c_str(), &jpgFilePath_);
      flipTexture_ = flipTexture;

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS ImageEncoder::process() {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      // Encoding with render texture
      if (!texturePtr_) {
        DEBUG_ERROR("Missing input texture pointer for screenshot");
        status = FBCAPTURE_GPU_ENCODER_NULL_TEXTURE_POINTER;
        return status;
      }

      if (!jpgFilePath_) {
        DEBUG_ERROR("Missing output path for screenshot");
        status = FBCAPTURE_OUTPUT_FILE_OPEN_FAILED;
        return status;
      }

      status = gpuEncoder->saveScreenShot(texturePtr_, jpgFilePath_, flipTexture_);
      if (status != FBCAPTURE_OK)
        DEBUG_ERROR_VAR("Failed saving screenshot", to_string(status));
      return status;
    }

    FBCAPTURE_STATUS ImageEncoder::finalize() {
      texturePtr_ = NULL;
      if (jpgFilePath_) {
        free(jpgFilePath_);
        jpgFilePath_ = NULL;
      }
      return FBCAPTURE_OK;
    }

    bool ImageEncoder::continueLoop() {
      return false;
    }
  }
}