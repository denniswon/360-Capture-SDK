/****************************************************************************************************************

Filename	:	AudioEncoder.cpp
Content		:
Copyright	:

****************************************************************************************************************/

#include "AudioEncoder.h"
#include "Common.h"
#include "Log.h"

namespace FBCapture {
  namespace Audio {

    AudioEncoder::AudioEncoder(FBCaptureEncoderDelegate *mainDelegate,
                               EncodePacketProcessorDelegate *processorDelegate,
                               const bool mute,
                               const bool mixMic,
                               const bool useRiftAudioSources) :
      FBCaptureEncoderModule(mainDelegate, processorDelegate),
      audioCapture_(NULL),
      audioEncoder_(NULL),
      encStatus_(EncStatus::ENC_NOT_ACCEPTING),
      mute_(mute),
      mixMic_(mixMic),
      useRiftAudioSources_(useRiftAudioSources),
      outputPath_(NULL) {}

    AudioEncoder::~AudioEncoder() {
      if (audioCapture_) {
        delete audioCapture_;
        audioCapture_ = NULL;
      }

      if (audioEncoder_) {
        delete audioEncoder_;
        audioEncoder_ = NULL;
      }
    }

    FBCAPTURE_STATUS AudioEncoder::init() {
      // Initialize audio capture intances that will capture raw input/output audio in wav format
      audioCapture_ = new AudioCapture();
      auto status = audioCapture_->initialize(mixMic_, useRiftAudioSources_);
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed initializing AudioCapture", to_string(status));
        return status;
      }

      // Initialize audio encoder intances that will encode the raw captured audio packets from wav to aac
      WAVEFORMATEX* pwfx = NULL;
      status = audioCapture_->getOutputWavFormat(&pwfx);
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed getting WAV format of audio capture output", to_string(status));
        return status;
      }

      audioEncoder_ = new MFAudioEncoder();
      status = audioEncoder_->initialize(pwfx, outputPath_);
      if (status != FBCAPTURE_OK)
        DEBUG_ERROR_VAR("Failed initializing AudioCapture", to_string(status));
      return status;
    }

    void AudioEncoder::mute(const bool mute) {
      mute_ = mute;
    }

    void AudioEncoder::setOutputPath(const string* dstFile) {
      ConvertToWide(const_cast<char*>((*dstFile).c_str()), const_cast<wchar_t**>(&outputPath_));
    }

    FBCAPTURE_STATUS AudioEncoder::getPacket(EncodePacket** packet) {
      FBCAPTURE_STATUS status = audioCapture_->captureAudio();
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed capturing raw audio packets", to_string(status));
        return status;
      }

      const float* buffer = nullptr;
      size_t numSamples = 0;
      uint64_t pts = 0;
      uint64_t duration = 0;

      status = audioCapture_->getOutputBuffer(&buffer, &numSamples, &pts, &duration, mute_);
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed AudioCapture::getOutputBuffer()", to_string(status));
        return status;
      }

      status = audioEncoder_->encode(buffer, static_cast<uint32_t>(numSamples), pts, duration, &encStatus_);
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed MFAudioEncoder::encode()", to_string(status));
        return status;
      }

      if (encStatus_ == EncStatus::ENC_NEED_MORE_INPUT) {
        status = FBCAPTURE_ENCODER_NEED_MORE_INPUT;
      } else if (encStatus_ == EncStatus::ENC_SUCCESS) {
        AudioEncodePacket* audioPacket;
        status = audioEncoder_->getEncodePacket(&audioPacket);
        if (status != FBCAPTURE_OK)
          DEBUG_ERROR_VAR("Failed MFAudioEncoder::getEncodePacket()", to_string(status));
        *packet = audioPacket;
      } else {
        // Should not reach here because failures are returned early above
        status = FBCAPTURE_AUDIO_ENCODER_UNEXPECTED_FAILURE;
      }

      return status;
    }

    FBCAPTURE_STATUS AudioEncoder::finalize() {
      auto status = audioCapture_->stopAudioCapture();
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed stopping AudioCapture", to_string(status));
        return status;
      }

      status = audioEncoder_->finalize();
      if (status != FBCAPTURE_OK)
        DEBUG_ERROR_VAR("Failed stopping MFAudioEncoder", to_string(status));

      if (outputPath_) {
        delete outputPath_;
        outputPath_ = NULL;
      }

      return status;
    }
  }
}