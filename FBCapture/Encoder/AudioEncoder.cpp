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
      bool mute,
      bool mixMic,
      bool useRiftAudioSources) :
      FBCaptureEncoderModule(mainDelegate, processorDelegate),
      encStatus_(EncStatus::ENC_NOT_ACCEPTING),
      mute_(mute),
      mixMic_(mixMic),
      useRiftAudioSources_(useRiftAudioSources),
      outputPath(NULL) {}

    AudioEncoder::~AudioEncoder() {
      if (audioCapture) {
        delete audioCapture;
        audioCapture = NULL;
      }

      if (audioEncoder) {
        delete audioEncoder;
        audioEncoder = NULL;
      }
    }

    FBCAPTURE_STATUS AudioEncoder::init() {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      // Initialize audio capture intances that will capture raw input/output audio in wav format
      audioCapture = new AudioCapture();
      status = audioCapture->initialize(mixMic_, useRiftAudioSources_);
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed initializing AudioCapture", to_string(status));
        return status;
      }

      // Initialize audio encoder intances that will encode the raw captured audio packets from wav to aac
      WAVEFORMATEX* pwfx = NULL;
      status = audioCapture->getOutputWavFormat(&pwfx);
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed getting WAV format of audio capture output", to_string(status));
        return status;
      }

      audioEncoder = new MFAudioEncoder();
      status = audioEncoder->initialize(pwfx);
      if (status != FBCAPTURE_OK)
        DEBUG_ERROR_VAR("Failed initializing AudioCapture", to_string(status));
      return status;
    }

    void AudioEncoder::mute(bool mute) {
      mute_ = mute;
    }

    void AudioEncoder::setOutputPath(const string* dstFile) {
      ConvertToWide((char*)(*dstFile).c_str(), (wchar_t**)&outputPath);
      audioEncoder->setOutputPath(outputPath);
    }

    FBCAPTURE_STATUS AudioEncoder::getPacket(EncodePacket** packet) {
      FBCAPTURE_STATUS status = audioCapture->captureAudio();
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed capturing raw audio packets", to_string(status));
        return status;
      }

      const float* buffer = nullptr;
      size_t numSamples = 0;
      uint64_t pts = 0;
      uint64_t duration = 0;

      status = audioCapture->getOutputBuffer(&buffer, &numSamples, &pts, &duration, mute_);
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed AudioCapture::getOutputBuffer()", to_string(status));
        return status;
      }

      status = audioEncoder->encode(buffer, (uint32_t)numSamples, pts, duration, &encStatus_);
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed MFAudioEncoder::encode()", to_string(status));
        return status;
      }

      if (encStatus_ == EncStatus::ENC_NEED_MORE_INPUT) {
        status = FBCAPTURE_ENCODER_NEED_MORE_INPUT;
      } else if (encStatus_ == EncStatus::ENC_SUCCESS) {
        AudioEncodePacket* audioPacket;
        status = audioEncoder->getEncodePacket(&audioPacket);
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
      FBCAPTURE_STATUS status = audioCapture->stopAudioCapture();
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed stopping AudioCapture", to_string(status));
        return status;
      }

      status = audioEncoder->finalize();
      if (status != FBCAPTURE_OK)
        DEBUG_ERROR_VAR("Failed stopping MFAudioEncoder", to_string(status));

      if (outputPath) {
        delete outputPath;
        outputPath = NULL;
      }

      return status;
    }
  }
}