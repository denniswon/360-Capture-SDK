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
                               const GRAPHICS_CARD_TYPE graphicsCardType,
                               ID3D11Device* device,
                               const bool enableAsyncMode) : FBCaptureModule(mainDelegate),
      gpuEncoder_(NULL),
      device_(device),
      graphicsCardType_(graphicsCardType),
      texturePtr_(NULL),
      jpgFilePath_(NULL),
      flipTexture_(false) {
      enableAsyncMode_ = enableAsyncMode;
    }

    ImageEncoder::~ImageEncoder() {
      ImageEncoder::finalize();
      if (gpuEncoder_)
        GPUEncoder::deleteInstance(&gpuEncoder_);
    }

    FBCAPTURE_STATUS ImageEncoder::init() {
      gpuEncoder_ = GPUEncoder::getInstance(graphicsCardType_, device_);
      if (!gpuEncoder_) {
        DEBUG_ERROR("Unsupported graphics card. The SDK supports only nVidia and AMD GPUs");
        return FBCAPTURE_GPU_ENCODER_UNSUPPORTED_DRIVER;
      };
      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS ImageEncoder::setInput(void *texturePtr, const DESTINATION_URL dstUrl, const bool flipTexture) {
      if (!texturePtr) {
        DEBUG_ERROR("It's invalid texture pointer: null");
        return FBCAPTURE_GPU_ENCODER_NULL_TEXTURE_POINTER;
      }

      texturePtr_ = texturePtr;
      if (dstUrl) {
        jpgFilePath_ = new wchar_t[MAX_FILENAME_LENGTH];
        wcscpy(jpgFilePath_, dstUrl);
      } else
        ConvertToWide(const_cast<char*>(GetDefaultOutputPath(kJpgExt).c_str()), &jpgFilePath_);
      flipTexture_ = flipTexture;

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS ImageEncoder::process() {
      // Encoding with render texture
      if (!texturePtr_) {
        DEBUG_ERROR("Missing input texture pointer for screenshot");
        return FBCAPTURE_GPU_ENCODER_NULL_TEXTURE_POINTER;
      }

      if (!jpgFilePath_) {
        DEBUG_ERROR("Missing output path for screenshot");
        return FBCAPTURE_OUTPUT_FILE_OPEN_FAILED;
      }

      auto status = gpuEncoder_->saveScreenShot(texturePtr_, jpgFilePath_, flipTexture_);
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