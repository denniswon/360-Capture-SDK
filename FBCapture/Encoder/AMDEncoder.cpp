/****************************************************************************************************************

Filename	:	AMDEncoder.cpp
Content   :	AMD Encoder implementation for creating h264 format video
Copyright	:

****************************************************************************************************************/

#include "AMDEncoder.h"
#include <AMD/include/components/VideoEncoderHEVC.h>

namespace FBCapture {
  namespace Video {

    PollingThread::PollingThread(amf::AMFContext *context, amf::AMFComponent *encoder, const char *pFileName) :
      context_(context),
      encoder_(encoder),
      file_(NULL) {
      if (pFileName)
        file_ = fopen(pFileName, "wb");
    }
    PollingThread::~PollingThread() {
      if (file_) {
        fclose(file_);
      }
    }
    void PollingThread::Run() {
      RequestStop();

      amf_pts latencyTime = 0;
      amf_pts writeDuration = 0;
      amf_pts encodeDuration = 0;
      amf_pts lastPollTime = 0;

      auto res = AMF_OK; // error checking can be added later
      while (true) {
        amf::AMFDataPtr data;
        res = encoder_->QueryOutput(&data);
        if (res == AMF_EOF) {
          break; // Drain complete
        }
        if (data != NULL) {
          const auto pollTime = amf_high_precision_clock();
          amf_pts startTime = 0;
          data->GetProperty(START_TIME_PROPERTY, &startTime);
          if (startTime < lastPollTime) // remove wait time if submission was faster then encode
          {
            startTime = lastPollTime;
          }
          lastPollTime = pollTime;

          encodeDuration += pollTime - startTime;

          if (latencyTime == 0) {
            latencyTime = pollTime - startTime;
          }

          amf_pts timestamp = 0;
          data->GetProperty(TIME_STAMP, &timestamp);
          data->SetPts(timestamp);

          amf::AMFBufferPtr buffer(data); // query for buffer interface
          if (file_)
            fwrite(buffer->GetNative(), 1, buffer->GetSize(), file_);

          writeDuration += amf_high_precision_clock() - pollTime;
        } else {
          amf_sleep(1);
        }
      }

      encoder_ = NULL;
      context_ = NULL;
    }

    AMDEncoder::AMDEncoder() :
      thread_(NULL),
      codec_(NULL),
      memoryTypeIn_(amf::AMF_MEMORY_DX11),
      formatIn_(amf::AMF_SURFACE_RGBA),
      widthIn_(0),
      heightIn_(0),
      maximumSpeed_(true),
      encodingConfigInitiated_(false),
      frameIdx_(0),
      tex_(NULL),
      newTex_(NULL),
      deviceDX11_(NULL),
      file_(NULL) {}

    AMDEncoder::~AMDEncoder() {
      if (thread_)
        delete thread_;

      surfaceIn_ = NULL;
      encoder_ = NULL;
      context_ = NULL;
      file_ = NULL;
    }

    FBCAPTURE_STATUS AMDEncoder::finalize() {
      while (true) {
        const auto res = encoder_->Drain();
        if (res != AMF_INPUT_FULL)  // handle full queue
        {
          break;
        }
        amf_sleep(1);  // input queue is full: wait and try again
      }

      if (thread_)
        thread_->WaitForStop();

      // cleanup in this order
      surfaceIn_ = NULL;
      if (encoder_)
        encoder_->Terminate();

      if (context_)
        context_->Terminate();

      encodingConfigInitiated_ = false;

      if (g_AMFFactory.GetFactory()) {
        const auto res = g_AMFFactory.Terminate();
        if (res != AMF_OK) {
          DEBUG_ERROR("Faield to release AMF resources");
          return FBCAPTURE_GPU_ENCODER_FINALIZE_FAILED;
        }
      }

      if (file_) {
        fclose(file_);
      }

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS AMDEncoder::initialization(void* texturePtr) {
      codec_ = AMFVideoEncoderVCE_AVC;
      memoryTypeIn_ = amf::AMF_MEMORY_DX11;
      formatIn_ = amf::AMF_SURFACE_RGBA;
      maximumSpeed_ = true;
      encodingConfigInitiated_ = false;

      if (g_AMFFactory.GetFactory() == NULL) {
        const auto hr = g_AMFFactory.Init();
        if (hr != AMF_OK) {
          DEBUG_ERROR("Failed to initialize AMF Factory. Display driver should be Crimson 17.1.1 or newer");
          return FBCAPTURE_GPU_ENCODER_UNSUPPORTED_DRIVER;
        }
      }

      ::amf_increase_timer_precision();
      // context
      if (context_ == NULL) {
        auto hr = g_AMFFactory.GetFactory()->CreateContext(&context_);
        if (hr != AMF_OK) {
          DEBUG_ERROR("Failed to create AMF context");
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
        if (memoryTypeIn_ == amf::AMF_MEMORY_DX11) {
          hr = context_->InitDX11(NULL); // can be DX11 device
          if (hr != AMF_OK) {
            DEBUG_ERROR("Failed to initiate dx11 device from AMFContext");
            return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
          }
        }
      }

      deviceDX11_ = static_cast<ID3D11Device*>(context_->GetDX11Device());

      D3D11_TEXTURE2D_DESC desc;
      tex_ = static_cast<struct ID3D11Texture2D*>(texturePtr);
      tex_->GetDesc(&desc);
      desc.BindFlags = 0;
      desc.MiscFlags &= D3D11_RESOURCE_MISC_TEXTURECUBE;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
      desc.Usage = D3D11_USAGE_STAGING;
      desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      widthIn_ = desc.Width;
      heightIn_ = desc.Height;

      // Call pure dx11 function(HRESULT) to create texture which will be used for copying RenderTexture to AMF buffer.
      auto hr = deviceDX11_->CreateTexture2D(&desc, NULL, &newTex_);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed to create texture 2d");
        return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
      }

      // component: encoder
      hr = g_AMFFactory.GetFactory()->CreateComponent(context_, codec_, &encoder_);
      if (hr != AMF_OK) {
        DEBUG_ERROR("Failed to create encoder component");
        return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
      }

      if (amf_wstring(codec_) == amf_wstring(AMFVideoEncoderVCE_AVC)) {
        hr = encoder_->SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_TRANSCONDING);
        if (hr != AMF_OK) {
          DEBUG_ERROR_VAR("Failed to set proprty: ", "AMF_VIDEO_ENCODER_USAGE & AMF_VIDEO_ENCODER_USAGE_TRANSCONDING");
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
        encoder_->SetProperty(AMF_VIDEO_ENCODER_PROFILE, AMF_VIDEO_ENCODER_PROFILE_BASELINE);
        if (hr != AMF_OK) {
          DEBUG_ERROR_VAR("Failed to set proprty: ", "AMF_VIDEO_ENCODER_PROFILE_LEVEL");
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
        if (maximumSpeed_) {
          hr = encoder_->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);
          if (hr != AMF_OK) {
            DEBUG_ERROR_VAR("Failed to set proprty: ", "AMF_VIDEO_ENCODER_B_PIC_PATTERN & 0");
            return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
          }
          hr = encoder_->SetProperty(AMF_VIDEO_ENCODER_QUALITY_PRESET, AMF_VIDEO_ENCODER_QUALITY_PRESET_SPEED);
          if (hr != AMF_OK) {
            DEBUG_ERROR_VAR("Failed to set proprty: ", "AMF_VIDEO_ENCODER_QUALITY_PRESET & AMF_VIDEO_ENCODER_QUALITY_PRESET_SPEED");
            return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
          }
        }

        hr = encoder_->SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, bitrate_);
        if (hr != AMF_OK) {
          DEBUG_ERROR_VAR("Failed to set proprty(AMF_VIDEO_ENCODER_TARGET_BITRATE) ", "bit rate: " + to_string(bitrate_));
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
        hr = encoder_->SetProperty(AMF_VIDEO_ENCODER_FRAMESIZE, ::AMFConstructSize(widthIn_, heightIn_));
        if (hr != AMF_OK) {
          DEBUG_ERROR_VAR("Failed to set proprty(AMF_VIDEO_ENCODER_FRAMESIZE) ", "width: " + to_string(widthIn_) + " " + "height: " + to_string(heightIn_));
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
        hr = encoder_->SetProperty(AMF_VIDEO_ENCODER_FRAMERATE, ::AMFConstructRate(fps_, 1));
        if (hr != AMF_OK) {
          DEBUG_ERROR_VAR("Failed to set proprty(AMF_VIDEO_ENCODER_FRAMERATE)", "frame rate: " + to_string(fps_));
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
        if (gop_ > 0) {
          hr = encoder_->SetProperty(AMF_VIDEO_ENCODER_IDR_PERIOD, gop_);
          if (hr != AMF_OK) {
            DEBUG_ERROR_VAR("Failed to set proprty(AMF_VIDEO_ENCODER_IDR_PERIOD)", "gop: " + to_string(gop_));
            return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
          }
        }

#if defined(ENABLE_4K)
        encoder->SetProperty(AMF_VIDEO_ENCODER_PROFILE, AMF_VIDEO_ENCODER_PROFILE_HIGH);
        hr = encoder->SetProperty(AMF_VIDEO_ENCODER_PROFILE_LEVEL, 51);
        if (hr != AMF_OK) {
          DEBUG_ERROR_VAR("Failed to set proprty: ", "AMF_VIDEO_ENCODER_PROFILE_LEVEL & 51");
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
        hr = encoder->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);
        if (hr != AMF_OK) {
          DEBUG_ERROR_VAR("Failed to set proprty: ", "AMF_VIDEO_ENCODER_B_PIC_PATTERN & 0");
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
#endif
      } else {
        hr = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_USAGE, AMF_VIDEO_ENCODER_HEVC_USAGE_TRANSCONDING);
        if (hr != AMF_OK) {
          DEBUG_ERROR_VAR("Failed to set proprty: ", "AMF_VIDEO_ENCODER_HEVC_USAGE & AMF_VIDEO_ENCODER_HEVC_USAGE_TRANSCONDING");
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }

        if (maximumSpeed_) {
          hr = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET, AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_SPEED);
          if (hr != AMF_OK) {
            DEBUG_ERROR_VAR("Failed to set proprty: ", "AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET & AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_SPEED");
            return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
          }
        }

        hr = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE, bitrate_);
        if (hr != AMF_OK) {
          DEBUG_ERROR_VAR("Failed to set proprty(AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE) ", "bit rate: " + to_string(bitrate_));
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
        hr = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_FRAMESIZE, ::AMFConstructSize(widthIn_, heightIn_));
        if (hr != AMF_OK) {
          DEBUG_ERROR_VAR("Failed to set proprty(AMF_VIDEO_ENCODER_HEVC_FRAMESIZE) ", "width: " + to_string(widthIn_) + " " + "height: " + to_string(heightIn_));
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
        hr = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_FRAMERATE, ::AMFConstructRate(fps_, 1));
        if (hr != AMF_OK) {
          DEBUG_ERROR_VAR("Failed to set proprty(AMF_VIDEO_ENCODER_HEVC_FRAMERATE)", "frame rate: " + to_string(fps_));
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
        if (gop_ > 0) {
          hr = encoder_->SetProperty(AMF_VIDEO_ENCODER_HEVC_GOP_SIZE, gop_);
          if (hr != AMF_OK) {
            DEBUG_ERROR_VAR("Failed to set proprty(AMF_VIDEO_ENCODER_HEVC_GOP_SIZE)", "gop: " + to_string(fps_));
            return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
          }
        }

#if defined(ENABLE_4K)
        encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_TIER, AMF_VIDEO_ENCODER_HEVC_TIER_HIGH);
        hr = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_PROFILE_LEVEL, AMF_LEVEL_5_1);
        if (hr != AMF_OK) {
          DEBUG_ERROR_VAR("Failed to set proprty(AMF_VIDEO_ENCODER_HEVC_PROFILE_LEVEL)", "AMF_LEVEL_5_1");
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
#endif
      }
      hr = encoder_->Init(formatIn_, widthIn_, heightIn_);
      if (hr != AMF_OK) {
        DEBUG_ERROR("Failed on initializing AMF components");
        return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
      }

//      thread_ = new PollingThread(context_, encoder_, outputPath_);
//      thread_->Start();

      encodingConfigInitiated_ = true;
      frameIdx_ = 0;

      // encode some frames
      DEBUG_LOG("Encoding configuration is initiated");

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS AMDEncoder::encode(void* texturePtr) {
      auto status = FBCAPTURE_OK;

      if (!encodingConfigInitiated_ && texturePtr) {
        status = initialization(texturePtr);
        if (status != FBCAPTURE_OK) {
          DEBUG_ERROR("Initial configuration setting is failed");
          return status;
        }
      }

      if (surfaceIn_ == NULL) {
        surfaceIn_ = NULL;
        const auto hr = context_->AllocSurface(memoryTypeIn_, formatIn_, widthIn_, heightIn_, &surfaceIn_);
        if (hr != AMF_OK) {
          DEBUG_ERROR("Failed to allocate surface");
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
        status = fillSurface(context_, surfaceIn_, flipTexture_);
        if (status != FBCAPTURE_OK) {
          DEBUG_ERROR("Failed to copy texture resources");
          return status;
        }
      }

      const auto tp = chrono::system_clock::now();
      if (firstFrame_) {
        firstFrameTp_ = tp;
        firstFrame_ = false;
        timestamp_ = 0;
      } else {
        timestamp_ = static_cast<uint64_t>(chrono::duration_cast<chrono::nanoseconds>(tp - firstFrameTp_).count()) / 100;
      }

      // encode
      const auto startTime = amf_high_precision_clock();
      surfaceIn_->SetProperty(START_TIME_PROPERTY, startTime);
      surfaceIn_->SetProperty(TIME_STAMP, timestamp_);

      if (gop_ > 0) {
        const auto type = frameIdx_ % gop_ == 0
          ? AMF_VIDEO_ENCODER_PICTURE_TYPE_I
          : AMF_VIDEO_ENCODER_PICTURE_TYPE_NONE;
        encoder_->SetProperty(AMF_VIDEO_ENCODER_FORCE_PICTURE_TYPE, type);
      }

      encoder_->SetProperty(AMF_VIDEO_ENCODER_INSERT_SPS, true);
      encoder_->SetProperty(AMF_VIDEO_ENCODER_INSERT_PPS, true);

      const auto hr = encoder_->SubmitInput(surfaceIn_);
      if (hr == AMF_INPUT_FULL) {  // handle full queue
        return FBCAPTURE_GPU_ENCODER_BUFFER_FULL;
      } else {
        surfaceIn_ = NULL;
      }

      frameIdx_++;

      return status;
    }

    FBCAPTURE_STATUS AMDEncoder::processOutput(void **buffer,
                                               uint32_t *length,
                                               uint64_t *timestamp,
                                               uint64_t *duration,
                                               uint32_t *frameIdx,
                                               bool *isKeyframe) {
      amf::AMFDataPtr data;
      const auto hr = encoder_->QueryOutput(&data);
      if (hr == AMF_EOF)
        return FBCAPTURE_GPU_ENCODER_BUFFER_EMPTY;

      if (hr != AMF_OK || !data)
        return FBCAPTURE_GPU_ENCODER_ENCODE_FRAME_FAILED;

      amf::AMFBufferPtr amfBuffer(data); // query for buffer interface

      *buffer = malloc(amfBuffer->GetSize());
      memcpy(*buffer, amfBuffer->GetNative(), amfBuffer->GetSize());
      *length = static_cast<uint32_t>(amfBuffer->GetSize());
      *timestamp = amfBuffer->GetPts();
      *frameIdx = frameIdx_;

      amf_int64 frameType;
      amfBuffer->GetProperty(AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE, &frameType);
      *isKeyframe = frameType == AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_IDR ||
        frameType == AMF_VIDEO_ENCODER_OUTPUT_DATA_TYPE_I;

      if (file_)
        fwrite(amfBuffer->GetNative(), 1, amfBuffer->GetSize(), file_);

      return FBCAPTURE_OK;
    }

    uint32_t AMDEncoder::getPendingCount() {
      auto encoder = amf::AMFComponentExPtr(encoder_);
      return encoder->GetInputCount();
    }

    FBCAPTURE_STATUS AMDEncoder::getSequenceParams(uint8_t **sps, uint32_t *spsLen, uint8_t **pps, uint32_t *ppsLen) {
//      amf::AMFVariant amfVar;
//      const auto hr = encoder_->GetProperty(AMF_VIDEO_ENCODER_EXTRADATA, amfVar.ToInterface());
//      if (hr != AMF_OK) {
//        DEBUG_ERROR("Failed to get encoder extra data preporty for sequence paramter output.");
//        return FBCAPTURE_GPU_ENCODER_GET_SEQUENCE_PARAMS_FAILED;
//      }
//
//      amf::AMFBufferPtr amfBuffer(amfVar.ToInterface());

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS AMDEncoder::fillSurface(amf::AMFContext *context, amf::AMFSurface *surface, const bool needFlipping) const {
      // Copy frame buffer to resource
      D3D11_MAPPED_SUBRESOURCE resource;
      const auto textureDX11 = static_cast<ID3D11Texture2D*>(surface->GetPlaneAt(0)->GetNative()); // no reference counting - do not Release()
      ID3D11DeviceContext *contextDX11 = NULL;

      deviceDX11_->GetImmediateContext(&contextDX11);
      contextDX11->CopyResource(newTex_, tex_);
      const auto subresource = D3D11CalcSubresource(0, 0, 0);
      auto hr = contextDX11->Map(newTex_, subresource, D3D11_MAP_READ_WRITE, 0, &resource);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed to map texture");
        return FBCAPTURE_GPU_ENCODER_MAP_INPUT_TEXTURE_FAILED;
      }

      // Flip pixels
      if (needFlipping) {
        const auto pixel = static_cast<unsigned char*>(resource.pData);
        const unsigned int rows = heightIn_ / 2;
        const unsigned int rowStride = widthIn_ * 4;
        const auto tmpRow = static_cast<unsigned char*>(malloc(rowStride));

        for (uint32_t rowIndex = 0; rowIndex < rows; rowIndex++) {
          const int sourceOffset = rowIndex * rowStride;
          const int targetOffset = (heightIn_ - rowIndex - 1) * rowStride;

          memcpy(tmpRow, pixel + sourceOffset, rowStride);
          memcpy(pixel + sourceOffset, pixel + targetOffset, rowStride);
          memcpy(pixel + targetOffset, tmpRow, rowStride);
        }

        free(tmpRow);
      }

      contextDX11->CopyResource(textureDX11, newTex_);

      contextDX11->Unmap(newTex_, 0);
      contextDX11->Flush();
      contextDX11->Release();

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS AMDEncoder::saveScreenShot(void* texturePtr, const DESTINATION_URL dstUrl, const bool flipTexture) {
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

      if (g_AMFFactory.GetFactory() == NULL) {
        const auto hr = g_AMFFactory.Init();
        if (hr != AMF_OK) {
          DEBUG_ERROR("Failed to initiate AMF");
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
      }

      // context
      if (context_ == NULL) {
        auto hr = g_AMFFactory.GetFactory()->CreateContext(&context_);
        if (hr != AMF_OK) {
          DEBUG_ERROR("Failed to create AMF context");
          return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
        }
        if (memoryTypeIn_ == amf::AMF_MEMORY_DX11) {
          hr = context_->InitDX11(NULL);
          if (hr != AMF_OK) {
            DEBUG_ERROR("Failed to init DX11 in AMF context");
            return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
          }
        }
      }

      auto deviceDX11 = static_cast<ID3D11Device*>(context_->GetDX11Device());
      ID3D11DeviceContext *contextDX11 = NULL;

      auto hr = deviceDX11->CreateTexture2D(&desc, NULL, &screenShotTex);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed to create texture 2d");
        return FBCAPTURE_GPU_ENCODER_INIT_FAILED;
      }

      D3D11_MAPPED_SUBRESOURCE resource;

      deviceDX11->GetImmediateContext(&contextDX11);
      contextDX11->CopyResource(screenShotTex, newScreenShotTex);
      const auto subresource = D3D11CalcSubresource(0, 0, 0);
      hr = contextDX11->Map(screenShotTex, subresource, D3D11_MAP_READ_WRITE, 0, &resource);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed to map texture");
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
      contextDX11->Unmap(screenShotTex, 0);

      // Screen Capture: Save texture to image file_
      hr = SaveWICTextureToFile(contextDX11, screenShotTex, GUID_ContainerFormatJpeg, dstUrl);
      if (FAILED(hr)) {
        DEBUG_ERROR("Failed to save texture to file_");
        return FBCAPTURE_GPU_ENCODER_WIC_SAVE_IMAGE_FAILED;
      }

      SAFE_RELEASE(screenShotTex);
      SAFE_RELEASE(newScreenShotTex);

      return FBCAPTURE_OK;
    }
  }
}
