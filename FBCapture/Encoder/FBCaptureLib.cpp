#include "FBCaptureLib.h"
#include "FBCaptureMain.h"
#include "delegate.h"

#define EXPECTED_STATUS(handle, expectedStatus) \
FBCAPTURE_STATUS sessionStatus = GetSessionStatus(handle); \
if (sessionStatus != expectedStatus) return FBCAPTURE_INVALID_FUNCTION_CALL;

#define EXPECTED_NOT_STATUS(handle, expectedNotStatus) \
FBCAPTURE_STATUS sessionStatus = GetSessionStatus(handle); \
if (sessionStatus == expectedNotStatus) return FBCAPTURE_INVALID_FUNCTION_CALL;

namespace FBCapture {

  ID3D11Device* FBCaptureMain::device = nullptr;
  GRAPHICS_CARD_TYPE FBCaptureMain::graphicsCardType = GRAPHICS_CARD_TYPE::UNKNOWN;

  FBCAPTURE_STATUS APIENTRY Create(FBCaptureConfig* config,
                                   FBCAPTURE_HANDLE* handle)

  {
    auto fb_capture = new FBCaptureMain();
    const auto status = fb_capture->initialize(config);
    if (status != FBCAPTURE_OK) {
      delete fb_capture;
      *handle = nullptr;
    } else
      *handle = fb_capture;
    return status;
  }

  FBCAPTURE_STATUS APIENTRY StartSession(FBCAPTURE_HANDLE handle,
                                         const wchar_t* dstUrl)

  {
    EXPECTED_STATUS(handle, FBCAPTURE_SESSION_INITIALIZED);
    FBCAPTURE_MAIN_DELEGATE(handle, &FBCaptureMain::startSession, dstUrl);
    return retStatus;
  }

  FBCAPTURE_STATUS APIENTRY EncodeFrame(FBCAPTURE_HANDLE handle,
                                        void* texturePtr)

  {
    EXPECTED_STATUS(handle, FBCAPTURE_SESSION_ACTIVE);
    FBCAPTURE_MAIN_DELEGATE(handle, &FBCaptureMain::encodeFrame, texturePtr);
    return retStatus;
  }

  FBCAPTURE_STATUS APIENTRY StopSession(FBCAPTURE_HANDLE handle)
  {
    EXPECTED_STATUS(handle, FBCAPTURE_SESSION_ACTIVE);
    FBCAPTURE_MAIN_DELEGATE(handle, &FBCaptureMain::stopSession);
    return retStatus;
  }

  FBCAPTURE_STATUS APIENTRY SaveScreenShot(FBCAPTURE_HANDLE handle,
                                           void *texturePtr,
                                           const wchar_t* dstUrl,
                                           bool flipTexture)
  {
    EXPECTED_STATUS(handle, FBCAPTURE_SESSION_INITIALIZED);
    FBCAPTURE_MAIN_DELEGATE(handle, &FBCaptureMain::saveScreenShot, texturePtr, dstUrl, flipTexture);
    return retStatus;
  }

  FBCAPTURE_STATUS APIENTRY Release(const FBCAPTURE_HANDLE handle)

  {
    const auto fbCapture = reinterpret_cast<FBCaptureMain*>(handle);
    if (!fbCapture)
      delete reinterpret_cast<FBCaptureMain*>(handle);
    return FBCAPTURE_OK;
  }

  FBCAPTURE_STATUS APIENTRY GetSessionStatus(const FBCAPTURE_HANDLE handle)

  {
    const auto fbCapture = reinterpret_cast<FBCaptureMain*>(handle);
    if (!fbCapture)
      return FBCAPTURE_INVALID_FUNCTION_CALL;
    return fbCapture->getSessionStatus();
  }

  FBCAPTURE_STATUS APIENTRY Mute(const FBCAPTURE_HANDLE handle,
                                 const bool mute)

  {
    const auto fbCapture = reinterpret_cast<FBCaptureMain*>(handle);
    if (!fbCapture ||
        !(fbCapture->getSessionStatus() == FBCAPTURE_SESSION_INITIALIZED ||
          fbCapture->getSessionStatus() == FBCAPTURE_SESSION_ACTIVE))
      return FBCAPTURE_INVALID_FUNCTION_CALL;

    return fbCapture->mute(mute);
  }

  void APIENTRY UnitySetGraphicsDevice(void* device,
                                       const int deviceType,
                                       int eventType)

  {
    const int nvidia_vender_id = 4318;  // NVIDIA Vendor ID: 0x10DE
    const int amd_vender_id1 = 4098;    // AMD Vendor ID: 0x1002
    const int amd_vender_id2 = 4130;    // AMD Vendor ID: 0x1022

                                // D3D11 device, remember device pointer and device type.
                                // The pointer we get is ID3D11Device.
    if (deviceType == GFX_DEVICE_RENDERER::GFX_RENDERER_D3_D11 && device != nullptr)
      FBCaptureMain::device = static_cast<ID3D11Device*>(device);

    IDXGIAdapter1* adapter;
    IDXGIFactory1* factory = nullptr;
    DXGI_ADAPTER_DESC1 adapterDescription;

    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
      DEBUG_ERROR("Failed to create DXGI factory object");
      return;
    }

    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
      adapter->GetDesc1(&adapterDescription);
      if (adapterDescription.VendorId == nvidia_vender_id)
        FBCaptureMain::graphicsCardType = GRAPHICS_CARD_TYPE::NVIDIA;
      else if (adapterDescription.VendorId == amd_vender_id1 || adapterDescription.VendorId == amd_vender_id2)
        FBCaptureMain::graphicsCardType = GRAPHICS_CARD_TYPE::AMD;
    }

    if (factory) {
      factory->Release();
      factory = nullptr;
    }
  }

}