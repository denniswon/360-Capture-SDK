/****************************************************************************************************************

Filename	:	NVEncoder.cpp
Content		:	NVidia Encoder implementation for creating h264 format video
Copyright	:

****************************************************************************************************************/

#include "NVidia/common/inc/nvFileIO.h"
#include "NVidia/common/inc/nvUtils.h"

#include "NVEncoder.h"

// To print out error string got from NV HW encoder API
const char* nvidiaStatus[] = { "NV_ENC_SUCCESS", "NV_ENC_ERR_NO_ENCODE_DEVICE", "NV_ENC_ERR_UNSUPPORTED_DEVICE", "NV_ENC_ERR_INVALID_ENCODERDEVICE",
"NV_ENC_ERR_INVALID_DEVICE", "NV_ENC_ERR_DEVICE_NOT_EXIST", "NV_ENC_ERR_INVALID_PTR", "NV_ENC_ERR_INVALID_EVENT",
"NV_ENC_ERR_INVALID_PARAM", "NV_ENC_ERR_INVALID_CALL", "NV_ENC_ERR_OUT_OF_MEMORY", "NV_ENC_ERR_ENCODER_NOT_INITIALIZED",
"NV_ENC_ERR_UNSUPPORTED_PARAM", "NV_ENC_ERR_LOCK_BUSY", "NV_ENC_ERR_NOT_ENOUGH_BUFFER", "NV_ENC_ERR_INVALID_VERSION",
"NV_ENC_ERR_MAP_FAILED", "NV_ENC_ERR_NEED_MORE_INPUT", "NV_ENC_ERR_ENCODER_BUSY", "NV_ENC_ERR_EVENT_NOT_REGISTERD",
"NV_ENC_ERR_GENERIC", "NV_ENC_ERR_INCOMPATIBLE_CLIENT_KEY", "NV_ENC_ERR_UNIMPLEMENTED", "NV_ENC_ERR_RESOURCE_REGISTER_FAILED",
"NV_ENC_ERR_RESOURCE_NOT_REGISTERED", "NV_ENC_ERR_RESOURCE_NOT_MAPPED" };

namespace FBCapture {
  namespace Video {

    NVEncoder::NVEncoder() {
      nvHWEncoder_ = new CNvHWEncoder;
      device_ = NULL;
      context_ = NULL;
      tex_ = NULL;
      newTex_ = NULL;
      encodingInitiated_ = false;
      encodeConfig_.fOutput = NULL;
      outputBuffer_ = NULL;
      outputBufferLength_ = 0;
    }

    NVEncoder::~NVEncoder() {
      destoryEncoder();
      if (nvHWEncoder_) {
        delete nvHWEncoder_;
        nvHWEncoder_ = NULL;
      }
    }

    NVENCSTATUS NVEncoder::initEncoder(D3D11_TEXTURE2D_DESC desc) {
      NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

      desc.BindFlags = 0;
      desc.MiscFlags &= D3D11_RESOURCE_MISC_TEXTURECUBE;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
      desc.Usage = D3D11_USAGE_STAGING;
      desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

      HRESULT hr = device_->CreateTexture2D(&desc, NULL, &newTex_);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed on creating new texture");
        return NV_ENC_ERR_GENERIC;
      }

      // Initialize Encoder
      nvStatus = nvHWEncoder_->Initialize(device_, NV_ENC_DEVICE_TYPE_DIRECTX);
      if (nvStatus == NV_ENC_ERR_INVALID_VERSION) {
        DEBUG_ERROR("Not supported NVidia graphics driver version. Driver version should be 379.95 or newer");
        return nvStatus;
      } else if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed on initializing encoder. [Error code]", nvidiaStatus[nvStatus]);
        return nvStatus;
      }

      // Set Encode Configs
      if (setEncodeConfigures(desc.Width, desc.Height) != FBCAPTURE_OK)
        return NV_ENC_ERR_GENERIC;
      else
        DEBUG_LOG("Set encode configurations successfully");

      nvStatus = nvHWEncoder_->CreateEncoder(&encodeConfig_);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed on creating encoder. [Error code]", nvidiaStatus[nvStatus]);
        return nvStatus;
      }

      nvStatus = allocateIOBuffers(encodeConfig_.width, encodeConfig_.height, encodeConfig_.inputFormat);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed on allocating IO buffers. [Error code]", nvidiaStatus[nvStatus]);
        return nvStatus;
      }

      encodingInitiated_ = true;

      return nvStatus;
    }

    FBCAPTURE_STATUS NVEncoder::setEncodeConfigures(uint32_t width, uint32_t height) {
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

      encodeConfig_.presetGUID = nvHWEncoder_->GetPresetGUID(encodeConfig_.encoderPreset, encodeConfig_.codec);

      DEBUG_LOG_VAR("Video Codec info", encodeConfig_.codec == NV_ENC_H264 ? "NV_ENC_H264" : "NV_ENC_HEVC");
      DEBUG_LOG_VAR("Bitrate", std::to_string(encodeConfig_.bitrate));
      DEBUG_LOG_VAR("FPS", std::to_string(encodeConfig_.fps));
      DEBUG_LOG_VAR("GOP Length", std::to_string(encodeConfig_.gopLength));
      DEBUG_LOG("Device type: DirectX 11");

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS NVEncoder::setGraphicsDeviceD3D11(ID3D11Device* device) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;
      device_ = device;
      HRESULT hr = createResources();
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed on setting DX11 graphics device for NVidia encoder.");
        status = FBCAPTURE_GPU_ENCODER_UNSUPPORTED_DRIVER;
      }
      return status;
    }

    void NVEncoder::releaseD3D11Resources() {
      SAFE_RELEASE(tex_);
      SAFE_RELEASE(newTex_);
      SAFE_RELEASE(context_);
      SAFE_RELEASE(vb_);
      SAFE_RELEASE(cb_);
      SAFE_RELEASE(vertexShader_);
      SAFE_RELEASE(pixelShader_);
      SAFE_RELEASE(inputLayout_);
      SAFE_RELEASE(rasterState_);
      SAFE_RELEASE(blendState_);
      SAFE_RELEASE(depthState_);
    }

    NVENCSTATUS NVEncoder::destoryEncoder() {
      NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

      releaseD3D11Resources();
      nvStatus = releaseIOBuffers();
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed on Releasing IO Buffers", nvidiaStatus[nvStatus]);
        return nvStatus;
      }

      if (encodeConfig_.fOutput) {
        fclose(encodeConfig_.fOutput);
      }

      if (nvHWEncoder_) {
        nvStatus = nvHWEncoder_->NvEncDestroyEncoder();
        if (nvStatus != NV_ENC_SUCCESS) {
          DEBUG_ERROR_VAR("Failed on NvEncDestroyEncoder", nvidiaStatus[nvStatus]);
          return nvStatus;
        }
      }

      if (outputBuffer_) {
        outputBuffer_ = NULL;
      }
      outputBufferLength_ = 0;

      firstFrame = true;
      timestamp = 0;

      return nvStatus;
    }

    NVENCSTATUS NVEncoder::releaseIOBuffers() {
      NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

      for (uint32_t i = 0; i < encodeBufferCount_; i++) {
        // Unmap each buffer input resources
        nvStatus = nvHWEncoder_->NvEncUnmapInputResource(encodeBuffer_[i].stInputBfr.hInputSurface);
        encodeBuffer_[i].stInputBfr.hInputSurface = NULL;

        if (encodeBuffer_[i].stInputBfr.pNVSurface) {
          encodeBuffer_[i].stInputBfr.pNVSurface->Release();
        }

        if (encodeBuffer_[i].stInputBfr.hInputSurface) {
          nvHWEncoder_->NvEncDestroyInputBuffer(encodeBuffer_[i].stInputBfr.hInputSurface);
          encodeBuffer_->stInputBfr.hInputSurface = NULL;
        }

        if (encodeBuffer_[i].stOutputBfr.hBitstreamBuffer) {
          nvHWEncoder_->NvEncDestroyBitstreamBuffer(encodeBuffer_[i].stOutputBfr.hBitstreamBuffer);
          encodeBuffer_->stOutputBfr.hBitstreamBuffer = NULL;
        }

        if (encodeBuffer_[i].stOutputBfr.hOutputEvent) {
          nvHWEncoder_->NvEncUnregisterAsyncEvent(encodeBuffer_[i].stOutputBfr.hOutputEvent);
          CloseHandle(encodeBuffer_[i].stOutputBfr.hOutputEvent);
          encodeBuffer_[i].stOutputBfr.hOutputEvent = NULL;
        }
      }

      if (eosOutputBfr_.hOutputEvent) {
        nvHWEncoder_->NvEncUnregisterAsyncEvent(eosOutputBfr_.hOutputEvent);
        CloseHandle(eosOutputBfr_.hOutputEvent);
        eosOutputBfr_.hOutputEvent = NULL;
      }

      return nvStatus;
    }

    NVENCSTATUS NVEncoder::finalizeEncodingSession() {
      NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

      if (eosOutputBfr_.hOutputEvent == NULL)
        return NV_ENC_ERR_INVALID_CALL;

      nvStatus = nvHWEncoder_->NvEncFinalize(eosOutputBfr_.hOutputEvent);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed on flush. [Error code]", nvidiaStatus[nvStatus]);
        return nvStatus;
      }

      // Encode buffer queue should have already been flushed before finalizing the encoding session
      EncodeBuffer *encodeBuffer = encodeBufferQueue_.getPending();
      if (encodeBuffer) {
        DEBUG_ERROR_VAR("Failed finalizing encoding session. The encoder needs to be flushed before finalizing.", nvidiaStatus[nvStatus]);
        return nvStatus;
      }

      if (WaitForSingleObject(eosOutputBfr_.hOutputEvent, 500) != WAIT_OBJECT_0) {
        nvStatus = NV_ENC_ERR_GENERIC;
      }

      return nvStatus;
    }

    NVENCSTATUS NVEncoder::allocateIOBuffers(uint32_t width, uint32_t height, NV_ENC_BUFFER_FORMAT inputFormat) {
      NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

      // Initialize encode buffer
      if (!encodeBufferQueue_.initialize(encodeBuffer_, encodeBufferCount_)) {
        DEBUG_ERROR_VAR("Failed initialize encode buffer queue of size", to_string(encodeBufferCount_));
        return NV_ENC_ERR_GENERIC;
      }

      for (uint32_t i = 0; i < encodeBufferCount_; i++) {

        // Create input buffer
        nvStatus = nvHWEncoder_->NvEncCreateInputBuffer(width, height, &encodeBuffer_[i].stInputBfr.hInputSurface, inputFormat);
        if (nvStatus != NV_ENC_SUCCESS) {
          DEBUG_ERROR_VAR("Creating input buffer has failed. [Error code]", nvidiaStatus[nvStatus]);
          return nvStatus;
        }

        encodeBuffer_[i].stInputBfr.bufferFmt = inputFormat;
        encodeBuffer_[i].stInputBfr.dwWidth = width;
        encodeBuffer_[i].stInputBfr.dwHeight = height;
        // Create output bit stream buffer
        nvStatus = nvHWEncoder_->NvEncCreateBitstreamBuffer(BITSTREAM_BUFFER_SIZE, &encodeBuffer_[i].stOutputBfr.hBitstreamBuffer);
        if (nvStatus != NV_ENC_SUCCESS) {
          DEBUG_ERROR_VAR("Creating bit stream buffer has failed. [Error code]", nvidiaStatus[nvStatus]);
          return nvStatus;
        }

        encodeBuffer_[i].stOutputBfr.dwBitstreamBufferSize = BITSTREAM_BUFFER_SIZE;
        // hOutputEvent
        nvStatus = nvHWEncoder_->NvEncRegisterAsyncEvent(&encodeBuffer_[i].stOutputBfr.hOutputEvent);
        if (nvStatus != NV_ENC_SUCCESS) {
          DEBUG_ERROR_VAR("Registering async event for encode buffer idx " + to_string(i) + " failed. [Error code]", nvidiaStatus[nvStatus]);
          return nvStatus;
        }

        encodeBuffer_[i].stOutputBfr.bWaitOnEvent = TRUE;
      }
      eosOutputBfr_.bEOSFlag = TRUE;

      nvStatus = nvHWEncoder_->NvEncRegisterAsyncEvent(&eosOutputBfr_.hOutputEvent);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Registering async event for eos output buffer failed. [Error code]", nvidiaStatus[nvStatus]);
        return nvStatus;
      }

      return nvStatus;
    }

    HRESULT NVEncoder::createResources() {
      HRESULT hr = S_OK;

      D3D11_BUFFER_DESC desc;
      memset(&desc, 0, sizeof(desc));

      // vertex buffer
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.ByteWidth = 1024;
      desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
      hr = device_->CreateBuffer(&desc, NULL, &vb_);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed to create buffer for vertex");
        return hr;
      }

      // constant buffer
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.ByteWidth = 64; // hold 1 matrix
      desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      desc.CPUAccessFlags = 0;
      hr = device_->CreateBuffer(&desc, NULL, &cb_);
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

    NVENCSTATUS NVEncoder::copyReources(EncodeBuffer *pEncodeBuffer, uint32_t width, uint32_t height, bool needFlipping) {
      NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

      D3D11_MAPPED_SUBRESOURCE resource;
      device_->GetImmediateContext(&context_);
      context_->CopyResource(newTex_, tex_);
      unsigned int subresource = D3D11CalcSubresource(0, 0, 0);

      HRESULT hr = context_->Map(newTex_, subresource, D3D11_MAP_READ_WRITE, 0, &resource);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed on context mapping");
        return NV_ENC_ERR_GENERIC;
      }

      // Flip pixels
      if (needFlipping) {
        unsigned char* pixel = (unsigned char*)resource.pData;
        const unsigned int rows = height / 2;
        const unsigned int rowStride = width * 4;
        unsigned char* tmpRow = (unsigned char*)malloc(rowStride);

        int source_offset, target_offset;

        for (uint32_t rowIndex = 0; rowIndex < rows; rowIndex++) {
          source_offset = rowIndex * rowStride;
          target_offset = (height - rowIndex - 1) * rowStride;

          memcpy(tmpRow, pixel + source_offset, rowStride);
          memcpy(pixel + source_offset, pixel + target_offset, rowStride);
          memcpy(pixel + target_offset, tmpRow, rowStride);
        }

        free(tmpRow);
        tmpRow = NULL;
      }

      // Unmap buffer
      context_->Unmap(newTex_, 0);

      // lock input buffer
      NV_ENC_LOCK_INPUT_BUFFER lockInputBufferParams = {};

      uint32_t pitch = 0;
      lockInputBufferParams.bufferDataPtr = NULL;

      nvStatus = nvHWEncoder_->NvEncLockInputBuffer(pEncodeBuffer->stInputBfr.hInputSurface, &lockInputBufferParams.bufferDataPtr, &pitch);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Creating nVidia input buffer failed. [Error code] ", nvidiaStatus[nvStatus]);
        return nvStatus;
      }

      // Write into Encode buffer
      memcpy(lockInputBufferParams.bufferDataPtr, resource.pData, height * resource.RowPitch);

      // Unlock input buffer
      nvStatus = nvHWEncoder_->NvEncUnlockInputBuffer(pEncodeBuffer->stInputBfr.hInputSurface);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed on nVidia unlock input buffer. [Error code] ", nvidiaStatus[nvStatus]);
        return nvStatus;
      }

      return nvStatus;
    }

    FBCAPTURE_STATUS NVEncoder::initialize(uint32_t bitrate, uint32_t fps, uint32_t gop, bool flipTexture, bool enableAsyncMode) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      status = GPUEncoder::initialize(bitrate, fps, gop, flipTexture, enableAsyncMode);
      if (status != FBCAPTURE_OK)
        goto exit;

      encodeBufferCount_ = enableAsyncMode_ ? MAX_ENCODE_QUEUE : 1;

      memset(&eosOutputBfr_, 0, sizeof(eosOutputBfr_));
      memset(&encodeBuffer_, 0, sizeof(encodeBuffer_) * encodeBufferCount_);
      memset(&encodeConfig_, 0, sizeof(EncodeConfig));

    exit:
      return status;
    }

    FBCAPTURE_STATUS NVEncoder::encode(const void* texturePtr) {
      int numFramesEncoded = 0;
      NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      if (!texturePtr)
        return FBCAPTURE_GPU_ENCODER_NULL_TEXTURE_POINTER;

      chrono::time_point<chrono::system_clock> tp = chrono::system_clock::now();
      if (firstFrame) {
        firstFrameTp = tp;
        firstFrame = false;
        timestamp = 0;
      } else {
        timestamp = (uint64_t)(chrono::duration_cast<chrono::nanoseconds>(tp - firstFrameTp).count()) / 100;
      }

      // Create new texture based on RenderTexture in Unity
      if (!encodingInitiated_ && texturePtr) {
        D3D11_TEXTURE2D_DESC desc;
        tex_ = (ID3D11Texture2D*)texturePtr;
        tex_->GetDesc(&desc);

        nvStatus = initEncoder(desc);
        if (nvStatus != NV_ENC_SUCCESS) {
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
      }

      EncodeBuffer *encodeBuffer = encodeBufferQueue_.getAvailable();
      if (!encodeBuffer) {
        nvStatus = NV_ENC_ERR_ENCODER_BUSY;
        DEBUG_ERROR_VAR("Encoding input buffer queue is full. Error code:", nvidiaStatus[nvStatus]);
        return FBCAPTURE_GPU_ENCODER_BUFFER_FULL;
      }

      // Copy frame buffer to encoding buffers
      nvStatus = copyReources(encodeBuffer, encodeConfig_.width, encodeConfig_.height, flipTexture_);
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

      return status;
    }

    FBCAPTURE_STATUS NVEncoder::processOutput(void **buffer, uint32_t *length, uint64_t *timestamp, uint64_t *duration, uint32_t *frameIdx, bool *isKeyframe) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;
      NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

      EncodeBuffer *encodeBuffer = encodeBufferQueue_.getPending();
      if (!encodeBuffer) {
        status = FBCAPTURE_GPU_ENCODER_BUFFER_EMPTY;
        return status;
      } else if (!encodeBuffer->stInputBfr.hInputSurface) {
        status = FBCAPTURE_GPU_ENCODER_PROCESS_OUTPUT_FAILED;
        DEBUG_ERROR_VAR("Encode input buffer surface null.", to_string(status));
        return status;
      }

      nvStatus = nvHWEncoder_->ProcessOutput(encodeBuffer, buffer, length, timestamp, duration, frameIdx, isKeyframe);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed processing output. [Error code] ", nvidiaStatus[nvStatus]);
        return FBCAPTURE_GPU_ENCODER_PROCESS_OUTPUT_FAILED;
      }

      outputBuffer_ = *buffer;
      outputBufferLength_ = *length;

      encodeBufferQueue_.decrementPendingCount();

      return status;
    }

    uint32_t NVEncoder::getPendingCount() {
      return encodeBufferQueue_.getPendingCount();
    }

    FBCAPTURE_STATUS NVEncoder::getSequenceParams(uint8_t **sps, uint32_t *spsLen, uint8_t **pps, uint32_t *ppsLen) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      // Retrieve Sequence Parameters
      uint32_t outSize = 0;
      char tmpHeader[NV_MAX_SEQ_HDR_LEN];
      NV_ENC_SEQUENCE_PARAM_PAYLOAD payload = { 0 };
      payload.version = NV_ENC_SEQUENCE_PARAM_PAYLOAD_VER;
      payload.spsppsBuffer = tmpHeader;
      payload.inBufferSize = sizeof(tmpHeader);
      payload.outSPSPPSPayloadSize = &outSize;

      NVENCSTATUS nvStatus = nvHWEncoder_->NvEncGetSequenceParams(&payload);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR("Failed on getting sequence paramters");
        return FBCAPTURE_GPU_ENCODER_GET_SEQUENCE_PARAMS_FAILED;
      }

      return status;
    }

    FBCAPTURE_STATUS NVEncoder::finalize() {
      NVENCSTATUS nvStatus;

      encodingInitiated_ = false;

      nvStatus = finalizeEncodingSession();
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed to finalize encoding session. [Error code] ", nvidiaStatus[nvStatus]);
        return FBCAPTURE_GPU_ENCODER_FINALIZE_FAILED;
      }

      nvStatus = destoryEncoder();
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed to release resources. [Error code] ", nvidiaStatus[nvStatus]);
        return FBCAPTURE_GPU_ENCODER_FINALIZE_FAILED;
      }

      return FBCAPTURE_OK;
    }

    NVENCSTATUS NVEncoder::encodeFrame(EncodeBuffer *pEncodeBuffer, uint32_t width, uint32_t height, NV_ENC_BUFFER_FORMAT inputformat) {
      NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

      uint32_t lockedPitch = 0;

      int8_t* qpDeltaMapArray = NULL;
      unsigned int qpDeltaMapArraySize = 0;

      nvStatus = nvHWEncoder_->NvEncEncodeFrame(pEncodeBuffer, NULL, width, height, timestamp, NV_ENC_PIC_STRUCT_FRAME, qpDeltaMapArray, qpDeltaMapArraySize);
      if (nvStatus != NV_ENC_SUCCESS) {
        DEBUG_ERROR_VAR("Failed on encoding frames. Error code:", nvidiaStatus[nvStatus]);
        return nvStatus;
      }

      return nvStatus;
    }

    FBCAPTURE_STATUS NVEncoder::saveScreenShot(const void* texturePtr, const DestinationURL dstUrl, bool flipTexture) {
      HRESULT hr = E_FAIL;
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      ID3D11Texture2D* newScreenShotTex;
      ID3D11Texture2D* screenShotTex;
      D3D11_TEXTURE2D_DESC desc;
      newScreenShotTex = (ID3D11Texture2D*)texturePtr;
      newScreenShotTex->GetDesc(&desc);
      desc.BindFlags = 0;
      desc.MiscFlags &= D3D11_RESOURCE_MISC_TEXTURECUBE;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
      desc.Usage = D3D11_USAGE_STAGING;
      desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

      hr = device_->CreateTexture2D(&desc, NULL, &screenShotTex);
      if (FAILED(hr)) {
        DEBUG_ERROR_VAR("Failed on creating new texture for ScreenShot. [Error code] ", to_string(hr));
        return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
      }

      D3D11_MAPPED_SUBRESOURCE resource;

      device_->GetImmediateContext(&context_);
      context_->CopyResource(screenShotTex, newScreenShotTex);
      unsigned int subresource = D3D11CalcSubresource(0, 0, 0);
      hr = context_->Map(screenShotTex, subresource, D3D11_MAP_READ_WRITE, 0, &resource);
      if (FAILED(hr)) {
        DEBUG_ERROR_VAR("Failed on context mapping. [Error code] ", to_string(hr));
        destoryEncoder();
        return FBCAPTURE_GPU_ENCODER_MAP_INPUT_TEXTURE_FAILED;
      }

      // Flip pixels
      if (flipTexture) {
        unsigned char* pixel = (unsigned char*)resource.pData;
        const unsigned int rows = desc.Height / 2;
        const unsigned int rowStride = desc.Width * 4;
        unsigned char* tmpRow = (unsigned char*)malloc(rowStride);

        int source_offset, target_offset;

        for (uint32_t rowIndex = 0; rowIndex < rows; rowIndex++) {
          source_offset = rowIndex * rowStride;
          target_offset = (desc.Height - rowIndex - 1) * rowStride;

          memcpy(tmpRow, pixel + source_offset, rowStride);
          memcpy(pixel + source_offset, pixel + target_offset, rowStride);
          memcpy(pixel + target_offset, tmpRow, rowStride);
        }

        free(tmpRow);
        tmpRow = NULL;
      }

      // Unmap buffer
      context_->Unmap(screenShotTex, 0);

      // Screen Capture: Save texture to image file
      hr = SaveWICTextureToFile(context_, screenShotTex, GUID_ContainerFormatJpeg, dstUrl);
      if (FAILED(hr)) {
        DEBUG_ERROR_VAR("Failed on creating image file. [Error code] ", to_string(hr));
        return FBCAPTURE_GPU_ENCODER_WIC_SAVE_IMAGE_FAILED;
      }

      SAFE_RELEASE(screenShotTex);
      SAFE_RELEASE(newScreenShotTex);

      DEBUG_LOG("Succeeded screen capture");

      return status;
    }
  }
}
