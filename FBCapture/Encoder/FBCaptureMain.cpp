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
    audioEncoder_(NULL),
    videoEncoder_(NULL),
    imageEncoder_(NULL),
    processor_(NULL),
    transmuxer_(NULL),
    terminateSignaled_(false),
    terminateStatus_(FBCAPTURE_OK),
    videoFinished_(false),
    audioFinished_(false),
    sessionStatus_(FBCAPTURE_OK) {}

  FBCaptureMain::~FBCaptureMain() {
    if (videoEncoder_)
      delete videoEncoder_;
    if (audioEncoder_)
      delete audioEncoder_;
    if (imageEncoder_)
      delete imageEncoder_;
    if (processor_)
      delete processor_;
    if (transmuxer_)
      delete transmuxer_;
    release();
  }

  FBCAPTURE_STATUS FBCaptureMain::initialize(FBCaptureConfig* config) {
    if (sessionStatus_ != FBCAPTURE_OK)
      return FBCAPTURE_INVALID_FUNCTION_CALL;

    processor_ = new EncodePacketProcessor();
    videoEncoder_ = new VideoEncoder(this, processor_,
                                    graphicsCardType, device,
                                    config->bitrate, config->fps, config->gop,
                                    config->flipTexture, config->enableAsyncMode);
    audioEncoder_ = new AudioEncoder(this, processor_,
                                    config->mute, config->mixMic, config->useRiftAudioSources);
    imageEncoder_ = new ImageEncoder(this,
                                    graphicsCardType, device, true);
    transmuxer_ = new Transmuxer(this, true);

    sessionStatus_ = FBCAPTURE_SESSION_INITIALIZED;
    return FBCAPTURE_OK;
  }

  FBCAPTURE_STATUS FBCaptureMain::startSession(const DESTINATION_URL dstUrl) {
    if (sessionStatus_ != FBCAPTURE_SESSION_INITIALIZED)
      return FBCAPTURE_INVALID_FUNCTION_CALL;

    frameCounter_.reset();

    auto status = processor_->initialize(dstUrl);
    if (status != FBCAPTURE_OK)
      goto exit;

    status = videoEncoder_->start();
    if (status != FBCAPTURE_OK)
      goto exit;

    const auto path = processor_->getOutputPath(kAacExt);
    audioEncoder_->setOutputPath(path);
    status = audioEncoder_->start();
    if (status != FBCAPTURE_OK)
      goto exit;

    status = transmuxer_->setInput(processor_->getOutputPath(kH264Ext),
                                  processor_->getOutputPath(kAacExt),
                                  processor_->getOutputPath(kMp4Ext));
    if (status != FBCAPTURE_OK)
      goto exit;

    activeSessionId_.increment();
    sessionStatus_ = FBCAPTURE_SESSION_ACTIVE;

  exit:
    if (status != FBCAPTURE_OK)
      onFailure(status);

    return status;
  }

  FBCAPTURE_STATUS FBCaptureMain::encodeFrame(void *texturePtr) {
    if (sessionStatus_ != FBCAPTURE_SESSION_ACTIVE &&
        sessionStatus_ != FBCAPTURE_SESSION_FAIL)
      return FBCAPTURE_INVALID_FUNCTION_CALL;

    if (terminateSignaled_.load())
      return terminateStatus_;

    const auto status = videoEncoder_->encode(texturePtr);
    if (status != FBCAPTURE_OK)
      onFailure(status);

    return status;
  }

  FBCAPTURE_STATUS FBCaptureMain::stopSession() {
    if (sessionStatus_ != FBCAPTURE_SESSION_ACTIVE &&
        sessionStatus_ != FBCAPTURE_SESSION_FAIL)
      return FBCAPTURE_INVALID_FUNCTION_CALL;

    if (terminateSignaled_.load())
      return terminateStatus_;

    auto status = audioEncoder_->stop();
    if (status != FBCAPTURE_OK)
      goto exit;

    status = videoEncoder_->stop();
    if (status != FBCAPTURE_OK)
      goto exit;

  exit:
    if (status != FBCAPTURE_OK)
      onFailure(status);

    return status;
  }

  FBCAPTURE_STATUS FBCaptureMain::saveScreenShot(void *texturePtr, DESTINATION_URL dstUrl, const bool flipTexture) {
    if (sessionStatus_ != FBCAPTURE_SESSION_INITIALIZED)
      return FBCAPTURE_INVALID_FUNCTION_CALL;

    auto status = imageEncoder_->setInput(texturePtr, dstUrl, flipTexture);
    if (status != FBCAPTURE_OK) {
      DEBUG_ERROR_VAR("Invalid input submitted for saving screenshot", ConvertToByte(dstUrl));
      return status;
    }

    status = imageEncoder_->start();
    if (status != FBCAPTURE_OK)
      goto exit;

    activeSessionId_.increment();

  exit:
    if (status != FBCAPTURE_OK)
      onFailure(status);

    return status;
  }

  FBCAPTURE_STATUS FBCaptureMain::mute(const bool mute) const {
    audioEncoder_->mute(mute);
    return FBCAPTURE_OK;
  }

  FBCAPTURE_STATUS FBCaptureMain::getSessionStatus() const {
    return sessionStatus_;
  }

  FBCAPTURE_STATUS FBCaptureMain::release() {
    activeSessionId_.reset();
    completedSessionId_.reset();
    terminateSignaled_ = false;
    terminateStatus_ = FBCAPTURE_OK;
    videoFinished_ = false;
    audioFinished_ = false;
    sessionStatus_ = FBCAPTURE_OK;
    frameCounter_.reset();
    RELEASE_LOG();
    return FBCAPTURE_OK;
  }

  void FBCaptureMain::onFailure(const FBCAPTURE_STATUS status) {
    terminateStatus_ = status;
    terminateSignaled_ = true;
    sessionStatus_ = FBCAPTURE_SESSION_FAIL;
    audioEncoder_->stop();
    videoEncoder_->stop();
    processor_->release();
  }

  void FBCaptureMain::onFinish(const PACKET_TYPE type) {
    switch (type) {
      case PACKET_TYPE::VIDEO:
        videoFinished_ = true;
        break;
      case PACKET_TYPE::AUDIO:
        audioFinished_ = true;
        break;
      default:
        break;
    }

    if (videoFinished_.load() && audioFinished_.load()) {
      processor_->finalize();

      const auto status = transmuxer_->start();
      if (status != FBCAPTURE_OK)
        onFailure(status);
    }
  }

  void FBCaptureMain::onFinish() {
    completedSessionId_.increment();
    const auto activeId = activeSessionId_.get();
    const auto completedId = completedSessionId_.get();

    if (activeId == completedId)
      sessionStatus_ = FBCAPTURE_SESSION_INITIALIZED;
    // the rest cases should not happen.
    else if (activeId > completedId)
      sessionStatus_ = FBCAPTURE_SESSION_ACTIVE;
    else {
      terminateStatus_ = FBCAPTURE_INVALID_SESSION_STATUS;
      sessionStatus_ = FBCAPTURE_SESSION_FAIL;
      terminateSignaled_ = true;
    }

    processor_->release();
  }
}
