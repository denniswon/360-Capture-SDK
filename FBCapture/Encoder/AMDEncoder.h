/****************************************************************************************************************

Filename	:	AMDEncoder.h
Content		:	AMD Encoder implementation for creating h264 format video
Copyright	:

****************************************************************************************************************/

#pragma once
#define _WINSOCKAPI_
#include <Windows.h>
#include <math.h>
#include <wincodec.h>
#include "AMD/common/AMFFactory.h"
#include "AMD/include/components/VideoEncoderVCE.h"
#include "AMD/include/components/VideoEncoderHEVC.h"
#include "AMD/common/Thread.h"
#include "AMD/common/AMFSTL.h"

#include "GPUEncoder.h"

#define START_TIME_PROPERTY L"StartTimeProperty" // custom property ID to store submission time in a frame - all custom properties are copied from input to output
#define TIME_STAMP L"TimeStamp"                  // custom property ID to store input time stamp of the frame
#define MILLISEC_TIME     10000

//--------------------------------------------------------------------------------------------------------------
// *** SAFE_RELEASE macro
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(a) if (a) { a->Release(); a= NULL; }
#endif

namespace FBCapture {
  namespace Video {

    class PollingThread : public amf::AMFThread {
    protected:
      amf::AMFContextPtr      context_;
      amf::AMFComponentPtr    encoder_;
      FILE                    *file_;
    public:
      PollingThread(amf::AMFContext *context, amf::AMFComponent *encoder, const char *pFileName);
      ~PollingThread();
      virtual void Run();
    };

    class AMDEncoder : public GPUEncoder {
    public:
      AMDEncoder();
      ~AMDEncoder();

      FBCAPTURE_STATUS encode(const void* texturePtr) override;
      FBCAPTURE_STATUS processOutput(void **buffer, uint32_t *length, uint64_t *timestamp, uint64_t *duration, uint32_t *frameIdx, bool *isKeyframe) override;
      FBCAPTURE_STATUS finalize() override;
      FBCAPTURE_STATUS saveScreenShot(const void* texturePtr, const DestinationURL dstUrl, bool flipTexture) override;
      FBCAPTURE_STATUS getSequenceParams(uint8_t **sps, uint32_t *spsLen, uint8_t **pps, uint32_t *ppsLen) override;
      uint32_t getPendingCount() override;

    protected:
      PollingThread* thread_;

      wchar_t *codec_;

      // initialize AMF
      amf::AMFContextPtr context_;
      amf::AMFComponentPtr encoder_;
      amf::AMFSurfacePtr surfaceIn_;

      amf::AMF_MEMORY_TYPE memoryTypeIn_;
      amf::AMF_SURFACE_FORMAT formatIn_;

      amf_int32 widthIn_;
      amf_int32 heightIn_;

      bool maximumSpeed_;
      bool encodingConfigInitiated_;

      amf_int32 frameIdx_;

      ID3D11Texture2D* tex_;
      ID3D11Texture2D* newTex_;
      ID3D11Device* deviceDX11_;

      FILE *file_;

    protected:
      FBCAPTURE_STATUS initialization(const void* texturePtr);
      FBCAPTURE_STATUS fillSurface(amf::AMFContext *context, amf::AMFSurface *surface, bool needFlipping);
    };
  }
}
