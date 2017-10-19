#pragma once

#include <string>
#include <chrono>
#include <stdio.h>
#if defined(WIN32)
#include <d3d11.h>
#include <wincodec.h>
#pragma warning(disable : 4996)
#endif

#include "EncodePacket.h"
#include "ScreenGrab.h"
#include "FBCaptureStatus.h"
#include "FileUtil.h"

using namespace std;
using namespace Directx;
using namespace FBCapture::Streaming;

namespace FBCapture {
  namespace Video {

    typedef enum {
      NVIDIA,
      AMD,
      UNKNOWN
    } GraphicsCardType;

    // Device type to be used for NV encoder initialization
    typedef enum {
      NV_ENC_DX9 = 0,
      NV_ENC_DX11 = 1,
      NV_ENC_CUDA = 2,
      NV_ENC_DX10 = 3,
    } NvEncodeDeviceType;

    // Event types for UnitySetGraphicsDevice
    typedef enum {
      kGfxDeviceEventInitialize = 0,
      kGfxDeviceEventShutdown = 1,
      kGfxDeviceEventBeforeReset = 2,
      kGfxDeviceEventAfterReset = 3,
    } GfxDeviceEventType;

    // Graphics device identifiers in Unity
    typedef enum {
      kGfxRendererOpenGL = 0, // OpenGL
      kGfxRendererD3D9,		// Direct3D 9
      kGfxRendererD3D11		// Direct3D 11
    } GfxDeviceRenderer;

    class GPUEncoder {
    protected:
      GPUEncoder();  // Constructor
      virtual ~GPUEncoder();  // Destructor

    public:
      static GPUEncoder* getInstance(GraphicsCardType type, ID3D11Device* device);
      static void deleteInstance(GPUEncoder** instance);

      // Set dx11 device pointer from Unity
      virtual FBCAPTURE_STATUS setGraphicsDeviceD3D11(ID3D11Device* device) {
        return FBCAPTURE_OK;
      }
      virtual FBCAPTURE_STATUS initialize(uint32_t bitrate, uint32_t fps, uint32_t gop, bool flipTexture, bool enableAsyncMode);
      // submit input texture ptr for encoding
      virtual FBCAPTURE_STATUS encode(const void* texturePtr) {
        return FBCAPTURE_OK;
      }
      // stop and finalize encoding sessions.
      virtual FBCAPTURE_STATUS finalize() {
        return FBCAPTURE_OK;
      }
      // get sequence parameters for a frame that was just encoded.
      virtual FBCAPTURE_STATUS getSequenceParams(uint8_t **sps, uint32_t *spsLen, uint8_t **pps, uint32_t *ppsLen) {
        return FBCAPTURE_OK;
      }

      virtual uint32_t getPendingCount() {
        return 0;
      }

      // Take screenshot using either NVEncoder or AMDEncoder depending on graphics card type
      virtual FBCAPTURE_STATUS saveScreenShot(const void* texturePtr, const DestinationURL dstUrl, bool flipTexture) {
        return FBCAPTURE_OK;
      }

      uint32_t getFps();
      uint32_t getBitrate();
      uint32_t getGop();
      FBCAPTURE_STATUS getEncodePacket(VideoEncodePacket** packet);

    protected:
      uint32_t bitrate_;
      uint32_t fps_;
      uint32_t gop_;
      bool flipTexture_;
      bool enableAsyncMode_;
      const void* outputBuffer_;
      uint32_t outputBufferLength_;

      uint64_t timestamp; // encoding timestamp in 100 nanosec unit. Set to 0 on the first frame.
      chrono::time_point<chrono::system_clock> firstFrameTp;
      bool firstFrame;

      // get output encoded data
      virtual FBCAPTURE_STATUS processOutput(void **buffer, uint32_t *length, uint64_t *timestamp, uint64_t *duration, uint32_t *frameIdx, bool *isKeyframe) {
        return FBCAPTURE_OK;
      }
    };
  }
}