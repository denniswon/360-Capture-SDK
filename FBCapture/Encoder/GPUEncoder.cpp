#include "GPUEncoder.h"
#include "NVEncoder.h"
#include "AMDEncoder.h"
#include "Log.h"

namespace FBCapture {
  namespace Video {

    GPUEncoder* GPUEncoder::getInstance(const GRAPHICS_CARD_TYPE type, ID3D11Device* device) {
      GPUEncoder* gpuEncoder = NULL;
      if (type == GRAPHICS_CARD_TYPE::NVIDIA && device) {
        gpuEncoder = new NVEncoder();
        // nvidia encoder sdk needs to pass d3d device pointer got from Unity
        const auto status = gpuEncoder->setGraphicsDeviceD3D11(device);
        if (status != FBCAPTURE_OK) {
          delete gpuEncoder;
          gpuEncoder = NULL;
        }
      } else if (type == GRAPHICS_CARD_TYPE::AMD)
        gpuEncoder = new AMDEncoder();

      return gpuEncoder;
    }

    void GPUEncoder::deleteInstance(GPUEncoder** instance) {
      delete *instance;
      *instance = NULL;
    }

    GPUEncoder::GPUEncoder() :
      bitrate_(NULL),
      fps_(NULL),
      gop_(NULL),
      flipTexture_(false),
      enableAsyncMode_(false),
      outputBuffer_(NULL),
      outputBufferLength_(0),
      timestamp_(0),
      firstFrame_(true) {}

    GPUEncoder::~GPUEncoder() {}

    FBCAPTURE_STATUS GPUEncoder::initialize(const uint32_t bitrate,
                                            const uint32_t fps,
                                            const uint32_t gop,
                                            const bool flipTexture,
                                            const bool enableAsyncMode) {
      bitrate_ = bitrate;
      fps_ = fps;
      gop_ = gop;
      flipTexture_ = flipTexture;
      enableAsyncMode_ = enableAsyncMode;
      return FBCAPTURE_OK;
    }

    uint32_t GPUEncoder::getFps() const {
      return fps_;
    }

    uint32_t GPUEncoder::getBitrate() const {
      return bitrate_;
    }

    uint32_t GPUEncoder::getGop() const {
      return gop_;
    }

    FBCAPTURE_STATUS GPUEncoder::getEncodePacket(VideoEncodePacket** packet) {
      void *buffer = nullptr;
      uint32_t length;
      uint64_t timestamp;
      uint64_t duration;
      uint32_t frameIdx;
      bool isKeyframe;

      auto status = processOutput(&buffer, &length, &timestamp, &duration, &frameIdx, &isKeyframe);
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed during processing encoded frame output", to_string(status));
        return status;
      }

      uint8_t *sps = nullptr, *pps = nullptr;
      uint32_t spsLen, ppsLen;
      status = getSequenceParams(&sps, &spsLen, &pps, &ppsLen);
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed getting sequence parameters for hardware encoder", to_string(status));
        return status;
      }

      // DEBUG_LOG_VAR("  Video packet pts (frameIdx:keyframe)", to_string((uint32_t)(timestamp / pow(10, 4))) + " ms (" + to_string(frameIdx) + ":" + to_string(isKeyframe) + ")");

      *packet = new VideoEncodePacket();
      (*packet)->buffer = static_cast<uint8_t*>(buffer);
      (*packet)->length = length;
      (*packet)->timestamp = timestamp;
      (*packet)->frameIdx = frameIdx;
      (*packet)->isKeyframe = isKeyframe;
      (*packet)->sps = sps;
      (*packet)->spsLen = spsLen;
      (*packet)->pps = pps;
      (*packet)->ppsLen = ppsLen;

      return status;
    }

  }
}
