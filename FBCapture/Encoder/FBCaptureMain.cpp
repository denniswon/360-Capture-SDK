/****************************************************************************************************************

Filename	:	FBCaptureMain.cpp
Content		:	Main controller for video and audio encoding, and muxing into final output with multithreading synchronization.
Copyright	:

****************************************************************************************************************/

#include "FBCaptureMain.h"
#include "Common.h"
#include "Log.h"

namespace FBCapture {

  FBCaptureMain::FBCaptureMain() :
    videoEncoder(NULL),
    audioEncoder(NULL),
    imageEncoder(NULL),
    processor(NULL),
    terminateSignaled(false),
    terminateStatus(FBCAPTURE_OK),
    videoFinished(false),
    audioFinished(false),
    sessionStatus(FBCAPTURE_OK) {}

  FBCaptureMain::~FBCaptureMain() {
    if (videoEncoder)
      delete videoEncoder;

    if (audioEncoder)
      delete audioEncoder;

    if (imageEncoder)
      delete imageEncoder;

    if (processor)
      delete processor;

    if (transmuxer)
      delete transmuxer;

    activeSessionID.reset();
    completedSessionID.reset();

    terminateSignaled = false;
    terminateStatus = FBCAPTURE_OK;

    videoFinished = false;
    audioFinished = false;

    sessionStatus = FBCAPTURE_OK;

    frameCounter.reset();

    RELEASE_LOG();
  }

  FBCAPTURE_STATUS FBCaptureMain::initialize(FBCaptureConfig* config) {
    if (sessionStatus != FBCAPTURE_OK)
      return FBCAPTURE_INVALID_FUNCTION_CALL;

    FBCAPTURE_STATUS status = FBCAPTURE_OK;

    processor = new EncodePacketProcessor();

    videoEncoder = new VideoEncoder(this, processor,
      graphicsCardType, device,
      config->bitrate, config->fps, config->gop,
      config->flipTexture, config->enableAsyncMode);

    audioEncoder = new AudioEncoder(this, processor,
      config->mute, config->mixMic, config->useRiftAudioSources);

    imageEncoder = new ImageEncoder(this,
      graphicsCardType, device, true);

    transmuxer = new Transmuxer(this, true);

    sessionStatus = FBCAPTURE_SESSION_INITIALIZED;
    return status;
  }

  FBCAPTURE_STATUS FBCaptureMain::startSession(DestinationURL dstUrl) {
    if (sessionStatus != FBCAPTURE_SESSION_INITIALIZED)
      return FBCAPTURE_INVALID_FUNCTION_CALL;

    FBCAPTURE_STATUS status = FBCAPTURE_OK;
    frameCounter.reset();

    status = processor->initialize(dstUrl);
    if (status != FBCAPTURE_OK)
      goto exit;

    status = videoEncoder->start();
    if (status != FBCAPTURE_OK)
      goto exit;

    audioEncoder->setOutputPath(processor->getOutputPath(kAacExt));
    status = audioEncoder->start();
    if (status != FBCAPTURE_OK)
      goto exit;

    status = transmuxer->setInput(processor->getOutputPath(kH264Ext),
      processor->getOutputPath(kAacExt),
      processor->getOutputPath(kMp4Ext));
    if (status != FBCAPTURE_OK)
      goto exit;

    activeSessionID.increment();
    sessionStatus = FBCAPTURE_SESSION_ACTIVE;

  exit:
    if (status != FBCAPTURE_OK)
      onFailure(status);

    return status;
  }

  FBCAPTURE_STATUS FBCaptureMain::encodeFrame(const void *texturePtr) {
    if (sessionStatus != FBCAPTURE_SESSION_ACTIVE &&
      sessionStatus != FBCAPTURE_SESSION_FAIL)
      return FBCAPTURE_INVALID_FUNCTION_CALL;

    if (terminateSignaled.load())
      return terminateStatus;

    FBCAPTURE_STATUS status = videoEncoder->encode(texturePtr);
    if (status != FBCAPTURE_OK)
      onFailure(status);

    return status;
  }

  FBCAPTURE_STATUS FBCaptureMain::stopSession() {
    if (sessionStatus != FBCAPTURE_SESSION_ACTIVE &&
      sessionStatus != FBCAPTURE_SESSION_FAIL)
      return FBCAPTURE_INVALID_FUNCTION_CALL;

    if (terminateSignaled.load())
      return terminateStatus;

    FBCAPTURE_STATUS status = FBCAPTURE_OK;
    status = audioEncoder->stop();
    if (status != FBCAPTURE_OK)
      goto exit;

    status = videoEncoder->stop();
    if (status != FBCAPTURE_OK)
      goto exit;

  exit:
    if (status != FBCAPTURE_OK)
      onFailure(status);

    return status;
  }

  FBCAPTURE_STATUS FBCaptureMain::saveScreenShot(const void *texturePtr, DestinationURL dstUrl, bool flipTexture) {
    if (sessionStatus != FBCAPTURE_SESSION_INITIALIZED)
      return FBCAPTURE_INVALID_FUNCTION_CALL;

    FBCAPTURE_STATUS status = imageEncoder->setInput(texturePtr, dstUrl, flipTexture);
    if (status != FBCAPTURE_OK) {
      DEBUG_ERROR_VAR("Invalid input submitted for saving screenshot", ConvertToByte(dstUrl));
      return status;
    }

    status = imageEncoder->start();
    if (status != FBCAPTURE_OK)
      goto exit;

    activeSessionID.increment();

  exit:
    if (status != FBCAPTURE_OK)
      onFailure(status);

    return status;
  }

  FBCAPTURE_STATUS FBCaptureMain::mute(bool mute) {
    audioEncoder->mute(mute);
    return FBCAPTURE_OK;
  }

  FBCAPTURE_STATUS FBCaptureMain::getSessionStatus() {
    return sessionStatus;
  }

  void FBCaptureMain::onFailure(FBCAPTURE_STATUS status) {
    terminateStatus = status;
    terminateSignaled = true;
    sessionStatus = FBCAPTURE_SESSION_FAIL;
    stopSession();
    processor->release();
  }

  void FBCaptureMain::onFinish(PacketType type) {
    switch (type) {
      case PacketType::VIDEO:
        videoFinished = true;
        break;
      case PacketType::AUDIO:
        audioFinished = true;
        break;
      default:
        break;
    }

    if (videoFinished.load() && audioFinished.load()) {
      processor->finalize();

      FBCAPTURE_STATUS status = transmuxer->start();
      if (status != FBCAPTURE_OK)
        onFailure(status);
    }
  }

  void FBCaptureMain::onFinish() {
    completedSessionID.increment();
    uint32_t activeID = activeSessionID.get();
    uint32_t completedID = completedSessionID.get();

    if (activeID == completedID)
      sessionStatus = FBCAPTURE_SESSION_INITIALIZED;
    // the rest cases should not happen.
    else if (activeID > completedID)
      sessionStatus = FBCAPTURE_SESSION_ACTIVE;
    else {
      terminateStatus = FBCAPTURE_INVALID_SESSION_STATUS;
      sessionStatus = FBCAPTURE_SESSION_FAIL;
      terminateSignaled = true;
    }

    processor->release();
  }
}
