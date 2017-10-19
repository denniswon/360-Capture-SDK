#pragma once

#define _WINSOCKAPI_
#include <windows.h>
#include <stdio.h>
#include <iostream>
#include <cstring>
#include <thread>
#include <condition_variable>
#include <mutex>
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
    FBCAPTURE_STATUS startSession(DestinationURL dstUrl);
    FBCAPTURE_STATUS encodeFrame(const void *texturePtr);
    FBCAPTURE_STATUS stopSession();
    FBCAPTURE_STATUS saveScreenShot(const void *texturePtr, DestinationURL dstUrl, bool flipTexture);
    FBCAPTURE_STATUS mute(bool mute);
    FBCAPTURE_STATUS getSessionStatus();

    /* FBCaptureDelegate */
    void onFinish();
    void onFailure(FBCAPTURE_STATUS status);
    /* FBCaptureEncoderDelegate */
    void onFinish(PacketType type);

  protected:
    // captures raw wav audio packet using AudioCapture and encode them to aac using MFAudioEncoder
    AudioEncoder* audioEncoder;

    // only used if enableAsyncMode_ is true for non-blocking video frame encoding.
    // synchronous mode video encoding is done by the main thread client that calls encodeFrame()
    VideoEncoder* videoEncoder;

    // Used for taking screenshot and encode to jpg image in async mode
    ImageEncoder* imageEncoder;

    // takes EncodePackets as inputs from the main (or the video thread if enableAsyncMode) and audio thread
    // and processes (saving to file, streaming) the encoded video/audio encoded packets onPacket() callback
    EncodePacketProcessor* processor;

    // muxes the h264 and aac audio, mux to mp4 then inject spherical video metadata
    Transmuxer* transmuxer;

    FrameCounter frameCounter;

    atomic<bool> terminateSignaled;           // set to true if failure has occurred during any stage of FBCapture session
    atomic<FBCAPTURE_STATUS> terminateStatus;

    atomic<bool> videoFinished;               // set to true when the video encoder thread finishes successfully
    atomic<bool> audioFinished;               // set to true when the audio encoder thread finishes successfully

    atomic<FBCAPTURE_STATUS> sessionStatus;

  protected:
    struct FBCaptureSessionID {
      atomic<uint32_t> sessionId_;

      FBCaptureSessionID() : sessionId_(0) {}
      FBCaptureSessionID(uint32_t sessionId) : sessionId_(sessionId) {}

      void increment() {
        ++sessionId_;
      }

      void reset() {
        sessionId_ = 0;
      }

      uint32_t get() {
        return sessionId_.load();
      }
    };

    FBCaptureSessionID activeSessionID;
    FBCaptureSessionID completedSessionID;

  public:
    static ID3D11Device* device;                     // DirectX device
    static GraphicsCardType graphicsCardType;        // GPU graphics card type

  };
}