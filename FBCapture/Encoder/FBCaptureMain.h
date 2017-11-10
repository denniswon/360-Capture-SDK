#pragma once

#include <atomic>

#include "FBCaptureConfig.h"
#include "EncodePacketProcessor.h"
#include "VideoEncoder.h"
#include "AudioEncoder.h"
#include "ImageEncoder.h"
#include "Transmuxer.h"
#include "FrameCounter.h"

using namespace FBCapture::Audio;
using namespace FBCapture::Video;
using namespace FBCapture::Mux;
using namespace FBCapture::Streaming;

namespace FBCapture {

  class FBCaptureMain : public FBCaptureEncoderDelegate {

  public:
    FBCaptureMain();
    ~FBCaptureMain();

  public:
    FBCAPTURE_STATUS initialize(FBCaptureConfig* config);
    FBCAPTURE_STATUS startSession(DESTINATION_URL dstUrl);
    FBCAPTURE_STATUS encodeFrame(void *texturePtr);
    FBCAPTURE_STATUS stopSession();
    FBCAPTURE_STATUS saveScreenShot(void *texturePtr, DESTINATION_URL dstUrl, bool flipTexture);
    FBCAPTURE_STATUS mute(bool mute) const;
    FBCAPTURE_STATUS getSessionStatus() const;
    FBCAPTURE_STATUS release();

    /* FBCaptureDelegate */
    void onFinish() override;
    void onFailure(FBCAPTURE_STATUS status) override;
    /* FBCaptureEncoderDelegate */
    void onFinish(PACKET_TYPE type) override;

  protected:
    // captures raw wav audio packet using AudioCapture and encode them to aac using MFAudioEncoder
    AudioEncoder* audioEncoder_;

    // only used if enableAsyncMode_ is true for non-blocking video frame encoding.
    // synchronous mode video encoding is done by the main thread client that calls encodeFrame()
    VideoEncoder* videoEncoder_;

    // Used for taking screenshot and encode to jpg image in async mode
    ImageEncoder* imageEncoder_;

    // takes EncodePackets as inputs from the main (or the video thread if enableAsyncMode) and audio thread
    // and processes (saving to file_, streaming) the encoded video/audio encoded packets onPacket() callback
    EncodePacketProcessor* processor_;

    // muxes the h264 and aac audio, mux to mp4 then inject spherical video metadata
    Transmuxer* transmuxer_;

    FrameCounter frameCounter_;

    atomic<bool> terminateSignaled_;           // set to true if failure has occurred during any stage of FBCapture session
    atomic<FBCAPTURE_STATUS> terminateStatus_;

    atomic<bool> videoFinished_;               // set to true when the video encoder thread finishes successfully
    atomic<bool> audioFinished_;               // set to true when the audio encoder thread finishes successfully

    atomic<FBCAPTURE_STATUS> sessionStatus_;

  protected:
    struct FBCaptureSessionId {
      atomic<uint32_t> sessionId;

      FBCaptureSessionId() : sessionId(0) {}
      explicit FBCaptureSessionId(const uint32_t sessionId) : sessionId(sessionId) {}

      void increment() {
        ++sessionId;
      }

      void reset() {
        sessionId = 0;
      }

      uint32_t get() const {
        return sessionId.load();
      }
    };

    FBCaptureSessionId activeSessionId_;
    FBCaptureSessionId completedSessionId_;

  public:
    static ID3D11Device* device;                     // DirectX device
    static GRAPHICS_CARD_TYPE graphicsCardType;      // GPU graphics card type

  };
}