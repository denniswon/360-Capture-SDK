#pragma once

#define _WINSOCKAPI_
#include <windows.h>

#include "FBCaptureStatus.h"
#include "FBCaptureConfig.h"

#define FBCAPTURE_LIB_EXPORT 1

#if defined(FBCAPTURE_LIB_EXPORT)
#   define FBCAPTURE_API __declspec(dllexport)
#else
#   define FBCAPTURE_API __declspec(dllimport)
#endif // FBCAPTURE_LIB_EXPORT

#ifdef __cplusplus
extern "C" {
#endif

  namespace FBCapture {

    typedef void* FBCAPTURE_HANDLE;

   /*
    * Function: FBCapture::Create()
    *
    * Initializes FBCaptureMain with appropriate hardware encoder and audio capture/encoder instances.
    * Required to be called before starting capture with FBCaptureConfig struct.
    *
    * This function will only work properly when the FBCapture session status is FBCAPTURE_OK.
    * On success, this api function sets the session status to FBCAPTURE_SESSION_INITIALIZED.
    * On failure, this api function sets the session status to SESSION_FAILURE.
    *
    * NOTE: For screenshot, FBCaptureConfig options do not apply as they are only applicable to video encoding/streaming.
    * If only interested in screenshot not video in the application, simply pass in an empty FBCaptureConfig object.
    */
    extern FBCAPTURE_API FBCAPTURE_STATUS APIENTRY Create(FBCaptureConfig* config, FBCAPTURE_HANDLE* handle);

    /*
    * Function: FBCapture::StartSession()
    *
    * Notifies the FBCapture processes of a video encoding/live streaming session starting.
    * Requires DestinationURL input, which can be either a file path or streaming server url.
    * FBCapture will determine if the session is capture or live streaming with the URL header.
    * For live streaming, rtmp://, http://, https://, rtp:// are identified, while all others will
    * be considered as capture and try to open the file local. If live streaming, this call will initialize
    * RTMP stream.
    *
    * If empty string or null is passed in as the DestinationURL for video capture, the mp4 output will be
    * save to the default directory 'GetCurrentDirectory()\FBCapture\' directory on Windows.
    *
    * This function will only work properly when the FBCapture session status is FBCAPTURE_SESSION_INITIALIZED.
    * On success, this api function sets the session status to FBCAPTURE_SESSION_ACTIVE.
    * On failure, this api function sets the session status to SESSION_FAILURE.
    */
    extern FBCAPTURE_API FBCAPTURE_STATUS APIENTRY StartSession(FBCAPTURE_HANDLE handle, const wchar_t* dstUrl);

    /*
     * Function: FBCaptureEncodeFrame()
     *
     * Encodes the given raw texture frame using hardware GPU accelerated encoder.
     * The texture pointer points to the frame render texture in the client application.
     *
     * This function will only work properly when the FBCapture session status is FBCAPTURE_SESSION_ACTIVE.
     * On failure, this api function sets the session status to SESSION_FAILURE.
     *
     * The client application is responsible for calling this function for every frame with the correct fps
     * set during itialization. If this function is not called with configured frame rate, a/v will be out of sync.
     */
    extern FBCAPTURE_API FBCAPTURE_STATUS APIENTRY EncodeFrame(FBCAPTURE_HANDLE handle, const void* texturePtr);

    /*
     * Function: FBCapture::StopSession():
     *
     * Stops the currently active FBCapture session (both video capture and live streaming).
     * For capture, this function triggers the muxing of video and audio into mp4,
     * then injects spherical video metadata to the video.
     * For livestreaming, rtmp connection is closed and streaming is stopped.
     *
     * Note that this function call only triggers finalization of the current FBCapture session.
     * The client is responsible for checking if the output finalization is completed by
     * calling GetSessionStatus() function and verify that the session status has been changed
     * from FBCAPTURE_SESSION_ACTIVE to FBCAPTURE_SESSION_INITIALIZED after finalization.
     *
     * This function will only work properly when the FBCapture session status is FBCAPTURE_SESSION_ACTIVE.
     * On success, this api function asynchronously sets the session status to FBCAPTURE_SESSION_INITIALIZED.
     * On failure, this api function asynchronously sets the session status to SESSION_FAILURE.
     */
    extern FBCAPTURE_API FBCAPTURE_STATUS APIENTRY StopSession(FBCAPTURE_HANDLE handle);

    /*
     * Function: FBCapture::SaveScreenShot():
     *
     * Encodes the frame render texture pointed by the texture pointer parameter into jpeg image file, and
     * save the image file to the specified destination jpg url path.
     * If flipTexture is true, then the captured screenshot will be flipped horizontally.
     *
     * If empty string or null is passed in as the DestinationURL for video capture, the jpg output will be
     * save to the default directory 'GetCurrentDirectory()\FBCapture\' directory on Windows.
     *
     * This function will only work properly when the FBCapture session status is FBCAPTURE_SESSION_INITIALIZED.
     * This api function sets the session status to FBCAPTURE_SESSION_ACTIVE, and then
     * on success, asynchronously sets the session status back to FBCAPTURE_SESSION_INITIALIZED.
     * On failure, it asynchronously sets the session status to SESSION_FAILURE.
     *
     * The client is responsible for checking if the screenshot session is completed by
     * calling GetSessionStatus() function and verify that the session status has been changed
     * from FBCAPTURE_SESSION_ACTIVE to FBCAPTURE_SESSION_INITIALIZED after completion.
     *
     * Note that SaveScreenShot() still requires Create() to be called beforehand,
     * but FBCaptureConfig options do not apply for screenshot as they are only for video sessions.

     */
    extern FBCAPTURE_API FBCAPTURE_STATUS APIENTRY SaveScreenShot(FBCAPTURE_HANDLE handle, const void *texturePtr, const wchar_t* dstUrl, bool flipTexture);

    /*
    * Function: FBCapture::Release():
    *
    * Safe releases resources allocated for FBCapture initialized.
    * Can only be called when the most recent encoding session is completed.
    *
    * Before calling this function, the client is responsible for checking
    * the most recent encoding session is completed by calling GetSessionStatus()
    * and verify that the session status is FBCAPTURE_SESSION_INITIALIZED.
    *
    * This api function sets the session status to FBCAPTURE_OK.
    */
    extern FBCAPTURE_API FBCAPTURE_STATUS APIENTRY Release(FBCAPTURE_HANDLE handle);

    /*
    * Function: FBCapture::GetSessionStatus():
    *
    * Gets the current FBCapture session status.
    */
    extern FBCAPTURE_API FBCAPTURE_STATUS APIENTRY GetSessionStatus(FBCAPTURE_HANDLE handle);

    /*
    * Function: FBCapture::Mute():
    *
    * API to mute/unmute audio capture at the start of or during the capture.
    * muteAudio true will mute both input/output audio source, false will unmute both.
    */
    extern FBCAPTURE_API FBCAPTURE_STATUS APIENTRY Mute(FBCAPTURE_HANDLE handle, bool mute);

    /*
     * Function: FBCapture::UnitySetGraphicsDevice():
     *
     * Called automatically by Unity when the DLL is loaded by the SurroundCapture script.
     * This function figures out the hardware GPU graphics card type (Nvidia vs. AMD) and initializes the corresponding
     * hardware encoder for the capture.
     */
    extern FBCAPTURE_API void APIENTRY UnitySetGraphicsDevice(void* device, int deviceType, int eventType);

  }

#ifdef __cplusplus
}
#endif