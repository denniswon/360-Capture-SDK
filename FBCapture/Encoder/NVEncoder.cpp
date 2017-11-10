/****************************************************************************************************************

Filename	:	NVEncoder.cpp
Content		:	NVidia Encoder implementation for creating h264 format video
Copyright	:

****************************************************************************************************************/

#include "NVidia/common/inc/nvFileIO.h"
#include "NVidia/common/inc/nvUtils.h"

#include "NVEncoder.h"

namespace FBCapture {
  namespace Video {

    NVEncoder::NVEncoder() :
      nvHwEncoder_(new CNvHWEncoder),
      encodeBufferCount_(0),
      picStruct_(0),
      outputBuffer_(NULL),
      outputBufferLength_(0),
      device_(NULL), context_(NULL), samplerState_(NULL),
      tex_(NULL), newTex_(NULL),
      vertextBuffer_(NULL), constBuffer_(NULL),
      vertexShader_(NULL), pixelShader_(NULL),
      inputLayout_(NULL),
      rasterState_(NULL), blendState_(NULL), depthState_(NULL),
      encodingInitiated_(false) {
      encodeConfig_.fOutput = NULL;
    }

    NVEncoder::~NVEncoder() {
      destoryEncoder();
      if (nvHwEncoder_) {
        delete nvHwEncoder_;
        nvHwEncoder_ = NULL;
      }
    }

    NVENCSTATUS NVEncoder::initEncoder(D3D11_TEXTURE2D_DESC desc) {
      desc.BindFlags = 0;
      desc.MiscFlags &= D3D11_RESOURCE_MISC_TEXTURECUBE;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
      desc.Usage = D3D11_USAGE_STAGING;
      desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

      auto hr = device_->CreateTexture2D(&desc, NULL, &newTex_);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed on creating new texture");
        return NV_ENC_ERR_GENERIC;
      }

      // Initialize Encoder
      auto nvStatus = nvHwEncoder_->Initialize(device_, NV_ENC_DEVICE_TYPE_DIRECTX);
      if (nvStatus == NV_ENC_ERR_INVALID_VERSION) {
        DEBUG_ERROR("Not supported NVidia graphics driver version. Driver version should be 379.95 or newer");
        return nvStatus;
      } else if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed on initializing encoder", to_string(nvStatus));
        return nvStatus;
      }

      // Set Encode Configs
      if (setEncodeConfigures(desc.Width, desc.Height) != FBCAPTURE_OK)
        return NV_ENC_ERR_GENERIC;
      else
        DEBUG_LOG("Set encode configurations successfully");

      nvStatus = nvHwEncoder_->CreateEncoder(&encodeConfig_);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed on creating encoder", to_string(nvStatus));
        return nvStatus;
      }

      nvStatus = allocateIOBuffers(encodeConfig_.width, encodeConfig_.height, encodeConfig_.inputFormat);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed on allocating IO buffers", to_string(nvStatus));
        return nvStatus;
      }

      encodingInitiated_ = true;

      return nvStatus;
    }

    FBCAPTURE_STATUS NVEncoder::setEncodeConfigures(const uint32_t width, const uint32_t height) {
      if (width == 0 || height == 0) {
        DEBUG_ERROR("Invalid texture pointer. Failed to set encoder configuration.");
        return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
      }

      // Set encoding configuration
      encodeConfig_.endFrameIdx = INT_MAX;
      encodeConfig_.bitrate = bitrate_;
      encodeConfig_.rcMode = NV_ENC_PARAMS_RC_CONSTQP;
      encodeConfig_.gopLength = gop_ < 0 ? NVENC_INFINITE_GOPLENGTH : gop_;
      encodeConfig_.deviceType = NV_ENC_DX11;
      encodeConfig_.codec = NV_ENC_H264;
      encodeConfig_.fps = fps_;
      encodeConfig_.qp = 28;
      encodeConfig_.i_quant_factor = DEFAULT_I_QFACTOR;
      encodeConfig_.b_quant_factor = DEFAULT_B_QFACTOR;
      encodeConfig_.i_quant_offset = DEFAULT_I_QOFFSET;
      encodeConfig_.b_quant_offset = DEFAULT_B_QOFFSET;
      encodeConfig_.presetGUID = NV_ENC_PRESET_DEFAULT_GUID;
      encodeConfig_.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
      encodeConfig_.inputFormat = NV_ENC_BUFFER_FORMAT_ABGR;
      encodeConfig_.width = width;
      encodeConfig_.height = height;
      encodeConfig_.enableAsyncMode = enableAsyncMode_;

      encodeConfig_.presetGUID = nvHwEncoder_->GetPresetGUID(encodeConfig_.encoderPreset, encodeConfig_.codec);

      DEBUG_LOG_VAR("Video Codec info", encodeConfig_.codec == NV_ENC_H264 ? "NV_ENC_H264" : "NV_ENC_HEVC");
      DEBUG_LOG_VAR("Bitrate", std::to_string(encodeConfig_.bitrate));
      DEBUG_LOG_VAR("FPS", std::to_string(encodeConfig_.fps));
      DEBUG_LOG_VAR("GOP Length", std::to_string(encodeConfig_.gopLength));
      DEBUG_LOG("Device type: DirectX 11");

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS NVEncoder::setGraphicsDeviceD3D11(ID3D11Device* device) {
      device_ = device;
      const auto hr = createResources();
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed on setting DX11 graphics device for NVidia encoder.");
        return FBCAPTURE_GPU_ENCODER_UNSUPPORTED_DRIVER;
      }
      return FBCAPTURE_OK;
    }

    void NVEncoder::releaseD3D11Resources() {
      SAFE_RELEASE(tex_);
      SAFE_RELEASE(newTex_);
      SAFE_RELEASE(context_);
      SAFE_RELEASE(vertextBuffer_);
      SAFE_RELEASE(constBuffer_);
      SAFE_RELEASE(vertexShader_);
      SAFE_RELEASE(pixelShader_);
      SAFE_RELEASE(inputLayout_);
      SAFE_RELEASE(rasterState_);
      SAFE_RELEASE(blendState_);
      SAFE_RELEASE(depthState_);
    }

    NVENCSTATUS NVEncoder::destoryEncoder() {
      releaseD3D11Resources();
      auto nvStatus = releaseIOBuffers();
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed on Releasing IO Buffers", to_string(nvStatus));
        return nvStatus;
      }

      if (encodeConfig_.fOutput) {
        fclose(encodeConfig_.fOutput);
      }

      if (nvHwEncoder_) {
        nvStatus = nvHwEncoder_->NvEncDestroyEncoder();
        if (nvStatus != NV_ENC_SUCCESS) {
          DEBUG_ERROR_VAR("Failed on NvEncDestroyEncoder", to_string(nvStatus));
          return nvStatus;
        }
      }

      if (outputBuffer_) {
        outputBuffer_ = NULL;
      }
      outputBufferLength_ = 0;

      firstFrame_ = true;
      timestamp_ = 0;

      return nvStatus;
    }

    NVENCSTATUS NVEncoder::releaseIOBuffers() {
      NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

      for (uint32_t i = 0; i < encodeBufferCount_; i++) {
        // Unmap each buffer input resources
        nvStatus = nvHwEncoder_->NvEncUnmapInputResource(encodeBuffer_[i].stInputBfr.hInputSurface);
        encodeBuffer_[i].stInputBfr.hInputSurface = NULL;

        if (encodeBuffer_[i].stInputBfr.pNVSurface) {
          encodeBuffer_[i].stInputBfr.pNVSurface->Release();
        }

        if (encodeBuffer_[i].stInputBfr.hInputSurface) {
          nvHwEncoder_->NvEncDestroyInputBuffer(encodeBuffer_[i].stInputBfr.hInputSurface);
          encodeBuffer_->stInputBfr.hInputSurface = NULL;
        }

        if (encodeBuffer_[i].stOutputBfr.hBitstreamBuffer) {
          nvHwEncoder_->NvEncDestroyBitstreamBuffer(encodeBuffer_[i].stOutputBfr.hBitstreamBuffer);
          encodeBuffer_->stOutputBfr.hBitstreamBuffer = NULL;
        }

        if (encodeBuffer_[i].stOutputBfr.hOutputEvent) {
          nvHwEncoder_->NvEncUnregisterAsyncEvent(encodeBuffer_[i].stOutputBfr.hOutputEvent);
          CloseHandle(encodeBuffer_[i].stOutputBfr.hOutputEvent);
          encodeBuffer_[i].stOutputBfr.hOutputEvent = NULL;
        }
      }

      if (eosOutputBfr_.hOutputEvent) {
        nvHwEncoder_->NvEncUnregisterAsyncEvent(eosOutputBfr_.hOutputEvent);
        CloseHandle(eosOutputBfr_.hOutputEvent);
        eosOutputBfr_.hOutputEvent = NULL;
      }

      return nvStatus;
    }

    NVENCSTATUS NVEncoder::finalizeEncodingSession() {
      if (eosOutputBfr_.hOutputEvent == NULL)
        return NV_ENC_ERR_INVALID_CALL;

      auto nvStatus = nvHwEncoder_->NvEncFinalize(eosOutputBfr_.hOutputEvent);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed on flush", to_string(nvStatus));
        return nvStatus;
      }

      // Encode buffer queue should have already been flushed before finalizing the encoding session
      const auto encodeBuffer = encodeBufferQueue_.getPending();
      if (encodeBuffer) {
        DEBUG_ERROR_VAR("Failed finalizing encoding session. The encoder needs to be flushed before finalizing.", to_string(nvStatus));
        return nvStatus;
      }

      if (WaitForSingleObject(eosOutputBfr_.hOutputEvent, 500) != WAIT_OBJECT_0) {
        nvStatus = NV_ENC_ERR_GENERIC;
      }

      return nvStatus;
    }

    NVENCSTATUS NVEncoder::allocateIOBuffers(const uint32_t width, const uint32_t height, const NV_ENC_BUFFER_FORMAT inputFormat) {
      // Initialize encode buffer
      if (!encodeBufferQueue_.initialize(encodeBuffer_, encodeBufferCount_)) {
        DEBUG_ERROR_VAR("Failed initialize encode buffer queue of size", to_string(encodeBufferCount_));
        return NV_ENC_ERR_GENERIC;
      }

      for (uint32_t i = 0; i < encodeBufferCount_; i++) {

        // Create input buffer
        auto nvStatus = nvHwEncoder_->NvEncCreateInputBuffer(width, height, &encodeBuffer_[i].stInputBfr.hInputSurface, inputFormat);
        if (nvStatus != NV_ENC_SUCCESS) {
          DEBUG_ERROR_VAR("Creating input buffer has failed", to_string(nvStatus));
          return nvStatus;
        }

        encodeBuffer_[i].stInputBfr.bufferFmt = inputFormat;
        encodeBuffer_[i].stInputBfr.dwWidth = width;
        encodeBuffer_[i].stInputBfr.dwHeight = height;
        // Create output bit stream buffer
        nvStatus = nvHwEncoder_->NvEncCreateBitstreamBuffer(BITSTREAM_BUFFER_SIZE, &encodeBuffer_[i].stOutputBfr.hBitstreamBuffer);
        if (nvStatus != NV_ENC_SUCCESS) {
          DEBUG_ERROR_VAR("Creating bit stream buffer has failed", to_string(nvStatus));
          return nvStatus;
        }

        encodeBuffer_[i].stOutputBfr.dwBitstreamBufferSize = BITSTREAM_BUFFER_SIZE;
        // hOutputEvent
        nvStatus = nvHwEncoder_->NvEncRegisterAsyncEvent(&encodeBuffer_[i].stOutputBfr.hOutputEvent);
        if (nvStatus != NV_ENC_SUCCESS) {
          DEBUG_ERROR_VAR("Registering async event for encode buffer idx " + to_string(i) + " failed", to_string(nvStatus));
          return nvStatus;
        }

        encodeBuffer_[i].stOutputBfr.bWaitOnEvent = true;
      }
      eosOutputBfr_.bEOSFlag = true;

      const auto nvStatus = nvHwEncoder_->NvEncRegisterAsyncEvent(&eosOutputBfr_.hOutputEvent);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Registering async event for eos output buffer failed", to_string(nvStatus));
        return nvStatus;
      }

      return nvStatus;
    }

    HRESULT NVEncoder::createResources() {
      D3D11_BUFFER_DESC desc;
      memset(&desc, 0, sizeof(desc));

      // vertex buffer
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.ByteWidth = 1024;
      desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
      auto hr = device_->CreateBuffer(&desc, NULL, &vertextBuffer_);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed to create buffer for vertex");
        return hr;
      }

      // constant buffer
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.ByteWidth = 64; // hold 1 matrix
      desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      desc.CPUAccessFlags = 0;
      hr = device_->CreateBuffer(&desc, NULL, &constBuffer_);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed to create buffer for constant");
        return hr;
      }

      // shaders
      hr = device_->CreateVertexShader(kVertexShaderCode, sizeof(kVertexShaderCode), nullptr, &vertexShader_);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed to create vertex shader");
        return hr;
      }
      hr = device_->CreatePixelShader(kPixelShaderCode, sizeof(kPixelShaderCode), nullptr, &pixelShader_);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed to create pixel shader");
        return hr;
      }

      // input layout
      if (vertexShader_) {
        D3D11_INPUT_ELEMENT_DESC s_DX11InputElementDesc[] =
        {
          { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
          { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        hr = device_->CreateInputLayout(s_DX11InputElementDesc, 2, kVertexShaderCode, sizeof(kVertexShaderCode), &inputLayout_);
        if (FAILED(hr)) {
          DEBUG_ERROR("Failed to create input layout");
          return hr;
        }
      }

      // render states
      D3D11_RASTERIZER_DESC rsdesc;
      memset(&rsdesc, 0, sizeof(rsdesc));
      rsdesc.FillMode = D3D11_FILL_SOLID;
      rsdesc.CullMode = D3D11_CULL_NONE;
      rsdesc.DepthClipEnable = TRUE;
      hr = device_->CreateRasterizerState(&rsdesc, &rasterState_);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed to create rasterizer state");
        return hr;
      }

      D3D11_DEPTH_STENCIL_DESC dsdesc;
      memset(&dsdesc, 0, sizeof(dsdesc));
      dsdesc.DepthEnable = TRUE;
      dsdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
      dsdesc.DepthFunc = getUsesReverseZ() ? D3D11_COMPARISON_GREATER_EQUAL : D3D11_COMPARISON_LESS_EQUAL;
      hr = device_->CreateDepthStencilState(&dsdesc, &depthState_);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed to create depth stencil state");
        return hr;
      }

      D3D11_BLEND_DESC bdesc;
      memset(&bdesc, 0, sizeof(bdesc));
      bdesc.RenderTarget[0].BlendEnable = FALSE;
      bdesc.RenderTarget[0].RenderTargetWriteMask = 0xF;
      hr = device_->CreateBlendState(&bdesc, &blendState_);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed to create blend state");
        return hr;
      }

      return hr;
    }

    NVENCSTATUS NVEncoder::copyReources(EncodeBuffer *pEncodeBuffer, const uint32_t width, const uint32_t height, const bool needFlipping) {
      D3D11_MAPPED_SUBRESOURCE resource;
      device_->GetImmediateContext(&context_);
      context_->CopyResource(newTex_, tex_);
      const auto subresource = D3D11CalcSubresource(0, 0, 0);

      const auto hr = context_->Map(newTex_, subresource, D3D11_MAP_READ_WRITE, 0, &resource);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed on context mapping");
        return NV_ENC_ERR_GENERIC;
      }

      // Flip pixels
      if (needFlipping) {
        const auto pixel = static_cast<unsigned char*>(resource.pData);
        const auto rows = height / 2;
        const auto rowStride = width * 4;
        const auto tmpRow = static_cast<unsigned char*>(malloc(rowStride));

        for (uint32_t rowIndex = 0; rowIndex < rows; rowIndex++) {
          const int sourceOffset = rowIndex * rowStride;
          const int targetOffset = (height - rowIndex - 1) * rowStride;

          memcpy(tmpRow, pixel + sourceOffset, rowStride);
          memcpy(pixel + sourceOffset, pixel + targetOffset, rowStride);
          memcpy(pixel + targetOffset, tmpRow, rowStride);
        }

        free(tmpRow);
      }

      // Unmap buffer
      context_->Unmap(newTex_, 0);

      // lock input buffer
      NV_ENC_LOCK_INPUT_BUFFER lockInputBufferParams;

      uint32_t pitch = 0;
      lockInputBufferParams.bufferDataPtr = NULL;

      auto nvStatus = nvHwEncoder_->NvEncLockInputBuffer(pEncodeBuffer->stInputBfr.hInputSurface, &lockInputBufferParams.bufferDataPtr, &pitch);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Creating nVidia input buffer failed ", to_string(nvStatus));
        return nvStatus;
      }

      // Write into Encode buffer
      memcpy(lockInputBufferParams.bufferDataPtr, resource.pData, height * resource.RowPitch);

      // Unlock input buffer
      nvStatus = nvHwEncoder_->NvEncUnlockInputBuffer(pEncodeBuffer->stInputBfr.hInputSurface);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed on nVidia unlock input buffer ", to_string(nvStatus));
        return nvStatus;
      }

      return nvStatus;
    }

    FBCAPTURE_STATUS NVEncoder::initialize(const uint32_t bitrate,
                                           const uint32_t fps,
                                           const uint32_t gop,
                                           const bool flipTexture,
                                           const bool enableAsyncMode) {
      const auto status = GPUEncoder::initialize(bitrate, fps, gop, flipTexture, enableAsyncMode);
      if (status != FBCAPTURE_OK)
        goto exit;

      encodeBufferCount_ = enableAsyncMode_ ? MAX_ENCODE_QUEUE : 1;

      memset(&eosOutputBfr_, 0, sizeof(eosOutputBfr_));
      memset(&encodeBuffer_, 0, sizeof(encodeBuffer_) * encodeBufferCount_);
      memset(&encodeConfig_, 0, sizeof(EncodeConfig));

    exit:
      return status;
    }

    FBCAPTURE_STATUS NVEncoder::encode(void* texturePtr) {
      if (!texturePtr)
        return FBCAPTURE_GPU_ENCODER_NULL_TEXTURE_POINTER;

      const auto tp = chrono::system_clock::now();
      if (firstFrame_) {
        firstFrameTp_ = tp;
        firstFrame_ = false;
        timestamp_ = 0;
      } else {
        timestamp_ = static_cast<uint64_t>(chrono::duration_cast<chrono::nanoseconds>(tp - firstFrameTp_).count()) / 100;
      }

      // Create new texture based on RenderTexture in Unity
      if (!encodingInitiated_ && texturePtr) {
        D3D11_TEXTURE2D_DESC desc;
        tex_ = static_cast<ID3D11Texture2D*>(texturePtr);
        tex_->GetDesc(&desc);

        const auto nvStatus = initEncoder(desc);
        if (nvStatus != NV_ENC_SUCCESS) {
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
      }

      auto *encodeBuffer = encodeBufferQueue_.getAvailable();
      if (!encodeBuffer) {
        const auto nvStatus = NV_ENC_ERR_ENCODER_BUSY;
        DEBUG_ERROR_VAR("Encoding input buffer queue is full", to_string(nvStatus));
        return FBCAPTURE_GPU_ENCODER_BUFFER_FULL;
      }

      // Copy frame buffer to encoding buffers
      auto nvStatus = copyReources(encodeBuffer, encodeConfig_.width, encodeConfig_.height, flipTexture_);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR("Failed on copying framebuffers to encode the input buffer.");
        return FBCAPTURE_GPU_ENCODER_MAP_INPUT_TEXTURE_FAILED;
      }

      // Encode
      nvStatus = encodeFrame(encodeBuffer, encodeConfig_.width, encodeConfig_.height, encodeConfig_.inputFormat);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR("Failed on encoding input frame buffer.");
        return FBCAPTURE_GPU_ENCODER_ENCODE_FRAME_FAILED;
      }

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS NVEncoder::processOutput(void **buffer,
                                              uint32_t *length,
                                              uint64_t *timestamp,
                                              uint64_t *duration,
                                              uint32_t *frameIdx,
                                              bool *isKeyframe) {
      const auto encodeBuffer = encodeBufferQueue_.getPending();
      if (!encodeBuffer)
        return FBCAPTURE_GPU_ENCODER_BUFFER_EMPTY;
      else if (!encodeBuffer->stInputBfr.hInputSurface) {
        const auto status = FBCAPTURE_GPU_ENCODER_PROCESS_OUTPUT_FAILED;
        DEBUG_ERROR_VAR("Encode input buffer surface null.", to_string(status));
        return status;
      }

      auto nvStatus = nvHwEncoder_->ProcessOutput(encodeBuffer, buffer, length, timestamp, duration, frameIdx, isKeyframe);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed processing output ", to_string(nvStatus));
        return FBCAPTURE_GPU_ENCODER_PROCESS_OUTPUT_FAILED;
      }

      outputBuffer_ = *buffer;
      outputBufferLength_ = *length;
      encodeBufferQueue_.decrementPendingCount();

      return FBCAPTURE_OK;
    }

    uint32_t NVEncoder::getPendingCount() {
      return encodeBufferQueue_.getPendingCount();
    }

    FBCAPTURE_STATUS NVEncoder::getSequenceParams(uint8_t **sps, uint32_t *spsLen, uint8_t **pps, uint32_t *ppsLen) {
      // Retrieve Sequence Parameters
      uint32_t outSize = 0;
      char tmpHeader[NV_MAX_SEQ_HDR_LEN];
      NV_ENC_SEQUENCE_PARAM_PAYLOAD payload = { 0 };
      payload.version = NV_ENC_SEQUENCE_PARAM_PAYLOAD_VER;
      payload.spsppsBuffer = tmpHeader;
      payload.inBufferSize = sizeof(tmpHeader);
      payload.outSPSPPSPayloadSize = &outSize;

      const auto nvStatus = nvHwEncoder_->NvEncGetSequenceParams(&payload);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR("Failed on getting sequence paramters");
        return FBCAPTURE_GPU_ENCODER_GET_SEQUENCE_PARAMS_FAILED;
      }

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS NVEncoder::finalize() {
      NVENCSTATUS nvStatus;

      encodingInitiated_ = false;

      nvStatus = finalizeEncodingSession();
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed to finalize encoding session ", to_string(nvStatus));
        return FBCAPTURE_GPU_ENCODER_FINALIZE_FAILED;
      }

      nvStatus = destoryEncoder();
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed to release resources ", to_string(nvStatus));
        return FBCAPTURE_GPU_ENCODER_FINALIZE_FAILED;
      }

      return FBCAPTURE_OK;
    }

    NVENCSTATUS NVEncoder::encodeFrame(EncodeBuffer *pEncodeBuffer,
                                       const uint32_t width,
                                       const uint32_t height,
                                       NV_ENC_BUFFER_FORMAT inputformat) const {
      int8_t* qpDeltaMapArray = NULL;
      const unsigned int qpDeltaMapArraySize = 0;

      auto nvStatus = nvHwEncoder_->NvEncEncodeFrame(pEncodeBuffer,
                                                     NULL,
                                                     width,
                                                     height,
                                                     timestamp_,
                                                     NV_ENC_PIC_STRUCT_FRAME,
                                                     qpDeltaMapArray,
                                                     qpDeltaMapArraySize);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed on encoding frames", to_string(nvStatus));
        return nvStatus;
      }

      return NV_ENC_SUCCESS;
    }

    FBCAPTURE_STATUS NVEncoder::saveScreenShot(void* texturePtr, const DESTINATION_URL dstUrl, const bool flipTexture) {
      ID3D11Texture2D* newScreenShotTex;
      ID3D11Texture2D* screenShotTex;
      D3D11_TEXTURE2D_DESC desc;
      newScreenShotTex = static_cast<ID3D11Texture2D*>(texturePtr);
      newScreenShotTex->GetDesc(&desc);
      desc.BindFlags = 0;
      desc.MiscFlags &= D3D11_RESOURCE_MISC_TEXTURECUBE;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
      desc.Usage = D3D11_USAGE_STAGING;
      desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

     auto hr = device_->CreateTexture2D(&desc, NULL, &screenShotTex);
      if (FAILED(hr)) {
        DEBUG_ERROR_VAR("Failed on creating new texture for ScreenShot ", to_string(hr));
        return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
      }

      D3D11_MAPPED_SUBRESOURCE resource;

      device_->GetImmediateContext(&context_);
      context_->CopyResource(screenShotTex, newScreenShotTex);
      const auto subresource = D3D11CalcSubresource(0, 0, 0);
      hr = context_->Map(screenShotTex, subresource, D3D11_MAP_READ_WRITE, 0, &resource);
      if (FAILED(hr)) {
        DEBUG_ERROR_VAR("Failed on context mapping ", to_string(hr));
        destoryEncoder();
        return FBCAPTURE_GPU_ENCODER_MAP_INPUT_TEXTURE_FAILED;
      }

      // Flip pixels
      if (flipTexture) {
        const auto pixel = static_cast<unsigned char*>(resource.pData);
        const auto rows = desc.Height / 2;
        const auto rowStride = desc.Width * 4;
        const auto tmpRow = static_cast<unsigned char*>(malloc(rowStride));

        for (uint32_t rowIndex = 0; rowIndex < rows; rowIndex++) {
          const int sourceOffset = rowIndex * rowStride;
          const int targetOffset = (desc.Height - rowIndex - 1) * rowStride;

          memcpy(tmpRow, pixel + sourceOffset, rowStride);
          memcpy(pixel + sourceOffset, pixel + targetOffset, rowStride);
          memcpy(pixel + sourceOffset, tmpRow, rowStride);
        }

        free(tmpRow);
      }

      // Unmap buffer
      context_->Unmap(screenShotTex, 0);

      // Screen Capture: Save texture to image file_
      hr = SaveWICTextureToFile(context_, screenShotTex, GUID_ContainerFormatJpeg, dstUrl);
      if (FAILED(hr)) {
        DEBUG_ERROR_VAR("Failed on creating image file_ ", to_string(hr));
        return FBCAPTURE_GPU_ENCODER_WIC_SAVE_IMAGE_FAILED;
      }

      SAFE_RELEASE(screenShotTex);
      SAFE_RELEASE(newScreenShotTex);

      DEBUG_LOG("Succeeded screen capture");

      return FBCAPTURE_OK;
    }
  }
}
