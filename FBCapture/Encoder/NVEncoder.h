/***************************************************************************************************************

Filename	:	NVEncoder.h
Content		:	NVidia Encoder implementation for creating h264 format video
Copyright	:

****************************************************************************************************************/
#pragma once

#include <iostream>
#include <stdlib.h>
#include <atomic>
#include <chrono>
#include <assert.h>
#include <direct.h>
#define GetCurrentDir _getcwd
#include "NVidia/common/inc/nvEncodeAPI.h"
#include "NVidia/common/inc/nvCPUOPSys.h"
#include "NVidia/common/inc/NvHWEncoder.h"

#include "GPUEncoder.h"

#define MAX_ENCODE_QUEUE 32

// SAFE_RELEASE macro
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(a) if (a) { a->Release(); a= NULL; }
#endif

// BITSTREAM_BUFFER_SIZE macro
#define BITSTREAM_BUFFER_SIZE 2 * 1024 * 1024

namespace FBCapture {
  namespace Video {

    // The class for queuing is used to manage encode buffers
    template<class T>
    class CNvQueue {
      T** buffer_;
      unsigned int size_;
      atomic<unsigned int> pendingCount_;
      unsigned int availableIdx_;
      unsigned int pendingIdx_;
    public:
      CNvQueue() : size_(0), availableIdx_(0), pendingIdx_(0), buffer_(NULL) {}

      ~CNvQueue() {
        delete[] buffer_;
      }

      bool initialize(T *items, unsigned int size) {
        size_ = size;
        pendingCount_ = 0;
        availableIdx_ = 0;
        pendingIdx_ = 0;
        buffer_ = new T *[size_];
        for (unsigned int i = 0; i < size_; i++) {
          buffer_[i] = &items[i];
        }
        return true;
      }

      T * getAvailable() {
        T *item = NULL;
        if (pendingCount_.load() == size_) {
          return NULL;
        }
        item = buffer_[availableIdx_];
        availableIdx_ = (availableIdx_ + 1) % size_;
        ++pendingCount_;
        return item;
      }

      T* getPending() {
        if (pendingCount_.load() == 0) {
          return NULL;
        }

        T *item = buffer_[pendingIdx_];
        pendingIdx_ = (pendingIdx_ + 1) % size_;
        return item;
      }

      void decrementPendingCount() {
        --pendingCount_;
      }

      int getPendingIndex() {
        return pendingIdx_;
      }

      int getPendingCount() {
        return pendingCount_.load();
      }
    };

    //
    // Which then was compiled with:
    // fxc /Tvs_4_0_level_9_3 /EVS source.hlsl /Fh outVS.h /Qstrip_reflect /Qstrip_debug /Qstrip_priv
    // fxc /Tps_4_0_level_9_3 /EPS source.hlsl /Fh outPS.h /Qstrip_reflect /Qstrip_debug /Qstrip_priv
    // and results pasted & formatted to take less lines here
    const BYTE kVertexShaderCode[] =
    {
      68, 88, 66, 67, 86, 189, 21, 50, 166, 106, 171, 1, 10, 62, 115, 48, 224, 137, 163, 129, 1, 0, 0, 0, 168, 2, 0, 0, 4, 0, 0, 0, 48, 0, 0, 0, 0, 1, 0, 0, 4, 2, 0, 0, 84, 2, 0, 0,
      65, 111, 110, 57, 200, 0, 0, 0, 200, 0, 0, 0, 0, 2, 254, 255, 148, 0, 0, 0, 52, 0, 0, 0, 1, 0, 36, 0, 0, 0, 48, 0, 0, 0, 48, 0, 0, 0, 36, 0, 1, 0, 48, 0, 0, 0, 0, 0,
      4, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 254, 255, 31, 0, 0, 2, 5, 0, 0, 128, 0, 0, 15, 144, 31, 0, 0, 2, 5, 0, 1, 128, 1, 0, 15, 144, 5, 0, 0, 3, 0, 0, 15, 128,
      0, 0, 85, 144, 2, 0, 228, 160, 4, 0, 0, 4, 0, 0, 15, 128, 1, 0, 228, 160, 0, 0, 0, 144, 0, 0, 228, 128, 4, 0, 0, 4, 0, 0, 15, 128, 3, 0, 228, 160, 0, 0, 170, 144, 0, 0, 228, 128,
      2, 0, 0, 3, 0, 0, 15, 128, 0, 0, 228, 128, 4, 0, 228, 160, 4, 0, 0, 4, 0, 0, 3, 192, 0, 0, 255, 128, 0, 0, 228, 160, 0, 0, 228, 128, 1, 0, 0, 2, 0, 0, 12, 192, 0, 0, 228, 128,
      1, 0, 0, 2, 0, 0, 15, 224, 1, 0, 228, 144, 255, 255, 0, 0, 83, 72, 68, 82, 252, 0, 0, 0, 64, 0, 1, 0, 63, 0, 0, 0, 89, 0, 0, 4, 70, 142, 32, 0, 0, 0, 0, 0, 4, 0, 0, 0,
      95, 0, 0, 3, 114, 16, 16, 0, 0, 0, 0, 0, 95, 0, 0, 3, 242, 16, 16, 0, 1, 0, 0, 0, 101, 0, 0, 3, 242, 32, 16, 0, 0, 0, 0, 0, 103, 0, 0, 4, 242, 32, 16, 0, 1, 0, 0, 0,
      1, 0, 0, 0, 104, 0, 0, 2, 1, 0, 0, 0, 54, 0, 0, 5, 242, 32, 16, 0, 0, 0, 0, 0, 70, 30, 16, 0, 1, 0, 0, 0, 56, 0, 0, 8, 242, 0, 16, 0, 0, 0, 0, 0, 86, 21, 16, 0,
      0, 0, 0, 0, 70, 142, 32, 0, 0, 0, 0, 0, 1, 0, 0, 0, 50, 0, 0, 10, 242, 0, 16, 0, 0, 0, 0, 0, 70, 142, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 16, 16, 0, 0, 0, 0, 0,
      70, 14, 16, 0, 0, 0, 0, 0, 50, 0, 0, 10, 242, 0, 16, 0, 0, 0, 0, 0, 70, 142, 32, 0, 0, 0, 0, 0, 2, 0, 0, 0, 166, 26, 16, 0, 0, 0, 0, 0, 70, 14, 16, 0, 0, 0, 0, 0,
      0, 0, 0, 8, 242, 32, 16, 0, 1, 0, 0, 0, 70, 14, 16, 0, 0, 0, 0, 0, 70, 142, 32, 0, 0, 0, 0, 0, 3, 0, 0, 0, 62, 0, 0, 1, 73, 83, 71, 78, 72, 0, 0, 0, 2, 0, 0, 0,
      8, 0, 0, 0, 56, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 7, 7, 0, 0, 65, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0,
      15, 15, 0, 0, 80, 79, 83, 73, 84, 73, 79, 78, 0, 67, 79, 76, 79, 82, 0, 171, 79, 83, 71, 78, 76, 0, 0, 0, 2, 0, 0, 0, 8, 0, 0, 0, 56, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      3, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 62, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 15, 0, 0, 0, 67, 79, 76, 79, 82, 0, 83, 86, 95, 80, 111, 115,
      105, 116, 105, 111, 110, 0, 171, 171
    };

    const BYTE kPixelShaderCode[] =
    {
      68, 88, 66, 67, 196, 65, 213, 199, 14, 78, 29, 150, 87, 236, 231, 156, 203, 125, 244, 112, 1, 0, 0, 0, 32, 1, 0, 0, 4, 0, 0, 0, 48, 0, 0, 0, 124, 0, 0, 0, 188, 0, 0, 0, 236, 0, 0, 0,
      65, 111, 110, 57, 68, 0, 0, 0, 68, 0, 0, 0, 0, 2, 255, 255, 32, 0, 0, 0, 36, 0, 0, 0, 0, 0, 36, 0, 0, 0, 36, 0, 0, 0, 36, 0, 0, 0, 36, 0, 0, 0, 36, 0, 1, 2, 255, 255,
      31, 0, 0, 2, 0, 0, 0, 128, 0, 0, 15, 176, 1, 0, 0, 2, 0, 8, 15, 128, 0, 0, 228, 176, 255, 255, 0, 0, 83, 72, 68, 82, 56, 0, 0, 0, 64, 0, 0, 0, 14, 0, 0, 0, 98, 16, 0, 3,
      242, 16, 16, 0, 0, 0, 0, 0, 101, 0, 0, 3, 242, 32, 16, 0, 0, 0, 0, 0, 54, 0, 0, 5, 242, 32, 16, 0, 0, 0, 0, 0, 70, 30, 16, 0, 0, 0, 0, 0, 62, 0, 0, 1, 73, 83, 71, 78,
      40, 0, 0, 0, 1, 0, 0, 0, 8, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 15, 15, 0, 0, 67, 79, 76, 79, 82, 0, 171, 171, 79, 83, 71, 78,
      44, 0, 0, 0, 1, 0, 0, 0, 8, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 83, 86, 95, 84, 65, 82, 71, 69, 84, 0, 171, 171
    };

    class NVEncoder : public GPUEncoder {
    public:
      NVEncoder();  // Constructor
      virtual ~NVEncoder();  // Destructor

      FBCAPTURE_STATUS setGraphicsDeviceD3D11(ID3D11Device* device) override;  // Set dx11 device pointer from Unity
      FBCAPTURE_STATUS initialize(uint32_t bitrate, uint32_t fps, uint32_t gop, bool flipTexture, bool enableAsyncMode) override;
      FBCAPTURE_STATUS encode(const void* texturePtr) override;
      FBCAPTURE_STATUS finalize() override;
      FBCAPTURE_STATUS processOutput(void **buffer, uint32_t *length, uint64_t *timestamp, uint64_t *duration, uint32_t *frameIdx, bool *isKeyframe) override;
      FBCAPTURE_STATUS getSequenceParams(uint8_t **sps, uint32_t *spsLen, uint8_t **pps, uint32_t *ppsLen) override;
      FBCAPTURE_STATUS saveScreenShot(const void* texturePtr, const DestinationURL dstUrl, bool flipTexture) override;
      uint32_t getPendingCount() override;

    protected:
      // To access the NVidia HW Encoder interfaces
      CNvHWEncoder *nvHWEncoder_;
      uint32_t encodeBufferCount_;
      uint32_t picStruct_;
      EncodeOutputBuffer eosOutputBfr_;
      EncodeBuffer encodeBuffer_[MAX_ENCODE_QUEUE];
      EncodeConfig encodeConfig_;
      CNvQueue<EncodeBuffer> encodeBufferQueue_;
      void* outputBuffer_;
      uint32_t outputBufferLength_;

      // DX 11 interfaces
      ID3D11Device* device_;
      ID3D11DeviceContext* context_;
      ID3D11SamplerState*	samplerState_;
      ID3D11Texture2D* tex_;
      ID3D11Texture2D* newTex_;
      ID3D11Buffer* vb_;  // Vertex Buffer
      ID3D11Buffer* cb_;  // Constant Buffer
      ID3D11VertexShader* vertexShader_;
      ID3D11PixelShader* pixelShader_;
      ID3D11InputLayout* inputLayout_;
      ID3D11RasterizerState* rasterState_;
      ID3D11BlendState* blendState_;
      ID3D11DepthStencilState* depthState_;

      bool encodingInitiated_;

    protected:
      // Initialize encoding input buffers and resources
      NVENCSTATUS initEncoder(D3D11_TEXTURE2D_DESC desc);

      // Set all configurations need to be set for encoding
      // It's called only once when starting encoding
      FBCAPTURE_STATUS setEncodeConfigures(uint32_t width, uint32_t height);

      // Allocate Buffers
      // Input buffers and bit steam buffers need to be allocated only once
      NVENCSTATUS allocateIOBuffers(uint32_t uInputWidth, uint32_t uInputHeight, NV_ENC_BUFFER_FORMAT inputFormat);

      // Copy textures to encode buffer
      NVENCSTATUS	copyReources(EncodeBuffer *pEncodeBuffer, uint32_t width, uint32_t height, bool needFlipping);

      // Encode images
      // This function should be called in the last stage in function calls
      // i.e. After allocating buffers and copying resources
      NVENCSTATUS encodeFrame(EncodeBuffer *pEncodeBuffer, uint32_t width, uint32_t height, NV_ENC_BUFFER_FORMAT inputformat);

      // Finalize the encoding session
      // This function will be called when encoding is done
      NVENCSTATUS finalizeEncodingSession();

      // Release all resources allocated for encoding
      NVENCSTATUS destoryEncoder();

      // Release allocated buffers in AllocateIOBuffers after flushing or terminating process
      NVENCSTATUS releaseIOBuffers();

      // Release resources related to DX11
      void releaseD3D11Resources();

      // Create vertex and constant buffers
      // Set render states
      HRESULT createResources();

      // Gets the feature level of the hardware device
      // https://msdn.microsoft.com/en-us/library/windows/desktop/ff476329(v=vs.85).aspx
      virtual bool getUsesReverseZ() {
        return (int)device_->GetFeatureLevel() >= (int)D3D_FEATURE_LEVEL_10_0;
      }
    };

    // NVEncodeAPI entry point
    typedef NVENCSTATUS(NVENCAPI *MYPROC)(NV_ENCODE_API_FUNCTION_LIST*);
  }
}
