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

  ID3D11Device* FBCaptureMain::device = NULL;
  GraphicsCardType FBCaptureMain::graphicsCardType = GraphicsCardType::UNKNOWN;

  FBCAPTURE_STATUS APIENTRY Create(FBCaptureConfig* config,
                                   FBCAPTURE_HANDLE* handle)

  {
    FBCaptureMain* fbCapture = new FBCaptureMain();
    FBCAPTURE_STATUS status = fbCapture->initialize(config);
    if (status != FBCAPTURE_OK) {
      delete fbCapture;
      *handle = NULL;
    } else
      *handle = fbCapture;
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
                                        const void* texturePtr)

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
                                           const void *texturePtr,
                                           const wchar_t* dstUrl,
                                           bool flipTexture)
  {
    EXPECTED_STATUS(handle, FBCAPTURE_SESSION_INITIALIZED);
    FBCAPTURE_MAIN_DELEGATE(handle, &FBCaptureMain::saveScreenShot, texturePtr, dstUrl, flipTexture);
    return retStatus;
  }

  FBCAPTURE_STATUS APIENTRY Release(FBCAPTURE_HANDLE handle)

  {
    FBCaptureMain* fbCapture = reinterpret_cast<FBCaptureMain*>(handle);
    if (!fbCapture)
      delete reinterpret_cast<FBCaptureMain*>(handle);
    return FBCAPTURE_OK;
  }

  FBCAPTURE_STATUS APIENTRY GetSessionStatus(FBCAPTURE_HANDLE handle)

  {
    FBCaptureMain* fbCapture = reinterpret_cast<FBCaptureMain*>(handle);
    if (!fbCapture)
      return FBCAPTURE_INVALID_FUNCTION_CALL;
    return fbCapture->getSessionStatus();
  }

  FBCAPTURE_STATUS APIENTRY Mute(FBCAPTURE_HANDLE handle,
                                 bool mute)

  {
    FBCaptureMain* fbCapture = reinterpret_cast<FBCaptureMain*>(handle);
    if (!fbCapture ||
        !(fbCapture->getSessionStatus() == FBCAPTURE_SESSION_INITIALIZED ||
          fbCapture->getSessionStatus() == FBCAPTURE_SESSION_ACTIVE))
      return FBCAPTURE_INVALID_FUNCTION_CALL;

    return fbCapture->mute(mute);
  }

  void APIENTRY UnitySetGraphicsDevice(void* device,
                                       int deviceType,
                                       int eventType)

  {
    int nvidiaVenderID = 4318;  // NVIDIA Vendor ID: 0x10DE
    int amdVenderID1 = 4098;    // AMD Vendor ID: 0x1002
    int amdVenderID2 = 4130;    // AMD Vendor ID: 0x1022

                                // D3D11 device, remember device pointer and device type.
                                // The pointer we get is ID3D11Device.
    if (deviceType == GfxDeviceRenderer::kGfxRendererD3D11 && device != NULL)
      FBCaptureMain::device = (ID3D11Device*)device;

    IDXGIAdapter1* adapter;
    vector<IDXGIAdapter1*> adapters;
    IDXGIFactory1* factory = NULL;
    DXGI_ADAPTER_DESC1 adapterDescription;

    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory))) {
      DEBUG_ERROR("Failed to create DXGI factory object");
      return;
    }

    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
      adapter->GetDesc1(&adapterDescription);
      if (adapterDescription.VendorId == nvidiaVenderID)
        FBCaptureMain::graphicsCardType = GraphicsCardType::NVIDIA;
      else if (adapterDescription.VendorId == amdVenderID1 || adapterDescription.VendorId == amdVenderID2)
        FBCaptureMain::graphicsCardType = GraphicsCardType::AMD;
    }

    if (factory) {
      factory->Release();
      factory = NULL;
    }
  }

}