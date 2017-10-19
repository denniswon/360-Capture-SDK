/****************************************************************************************************************

Filename	:	VideoEncoder.cpp
Content		:
Copyright	:

****************************************************************************************************************/

#include "VideoEncoder.h"

namespace FBCapture {
  namespace Video {

    VideoEncoder::VideoEncoder(FBCaptureEncoderDelegate *mainDelegate,
                               EncodePacketProcessorDelegate *processorDelegate,
                               GraphicsCardType graphicsCardType,
                               ID3D11Device* device,
                               uint32_t bitrate,
                               uint32_t fps,
                               uint32_t gop,
                               bool flipTexture,
                               bool enableAsyncMode) :
      FBCaptureEncoderModule(mainDelegate, processorDelegate),
      graphicsCardType_(graphicsCardType),
      device_(device),
      bitrate_(bitrate),
      fps_(fps),
      gop_(gop),
      flipTexture_(flipTexture) {
      enableAsyncMode_ = enableAsyncMode;
    }

    VideoEncoder::~VideoEncoder() {
      if (gpuEncoder_)
        GPUEncoder::deleteInstance(&gpuEncoder_);
    }

    FBCAPTURE_STATUS VideoEncoder::init() {
      // Initialize hardware-accelerated video encoder that encodes input texture frame to h264 packets
      gpuEncoder_ = GPUEncoder::getInstance(graphicsCardType_, device_);
      if (!gpuEncoder_) {
        DEBUG_ERROR("Unsupported graphics card. The SDK supports only nVidia and AMD GPUs");
        return FBCAPTURE_GPU_ENCODER_UNSUPPORTED_DRIVER;
      }

      FBCAPTURE_STATUS status = gpuEncoder_->initialize(bitrate_, fps_, gop_, flipTexture_, enableAsyncMode_);
      if (status != FBCAPTURE_OK)
        DEBUG_ERROR_VAR("Failed initializing hardware encoder", to_string(status));
      return status;
    }

    FBCAPTURE_STATUS VideoEncoder::encode(const void *texturePtr) {
      if (!texturePtr) {
        DEBUG_ERROR("It's invalid texture pointer: null");
        return FBCAPTURE_GPU_ENCODER_NULL_TEXTURE_POINTER;
      }

      FBCAPTURE_STATUS status = gpuEncoder_->encode(texturePtr);
      if (status != FBCAPTURE_OK || enableAsyncMode_)
        return status;

      return process();
    }

    FBCAPTURE_STATUS VideoEncoder::getPacket(EncodePacket** packet) {
      VideoEncodePacket* videoPacket;

      FBCAPTURE_STATUS status = gpuEncoder_->getEncodePacket(&videoPacket);
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed creating VideoEncodePacket from Video Encoder", to_string(status));
        return status;
      }

      *packet = videoPacket;
      return status;
    }

    FBCAPTURE_STATUS VideoEncoder::finalize() {
      // Finalize the hardware encoding session after flushing
      FBCAPTURE_STATUS status = gpuEncoder_->finalize();
      if (status != FBCAPTURE_OK)
        DEBUG_ERROR_VAR("Failed flushing the hardware encoder", to_string(status));
      return status;
    }
  }
}