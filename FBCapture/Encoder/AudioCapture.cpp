/****************************************************************************************************************

Filename	:	AudioCapture.cpp
Content		:
Copyright	:

****************************************************************************************************************/

#include "AudioCapture.h"
#include "Common.h"
#include "Log.h"
#include "Functiondiscoverykeys_devpkey.h"

#define REFTIMES_PER_SEC  10000000

namespace FBCapture {
  namespace Audio {

    AudioCapture::AudioCapture() :
      deviceEnumerator_(NULL),
      inputAudioSource_(NULL),
      outputAudioClient_(NULL),
      outputAudioCaptureClient_(NULL),
      outputAudioClock_(NULL),
      outputAudioDevice_(NULL),
      outputPwfx_(NULL),
      outputAudioSource_(NULL),
      inputAudioClient_(NULL),
      inputAudioCaptureClient_(NULL),
      inputAudioClock_(NULL),
      inputAudioDevice_(NULL),
      inputPwfx_(NULL),
      file_(NULL),
      outputFile_(NULL),
      mixMic_(false),
      totalOutputNumSamples_(0),
      totalOutputBytesLength_(0),
      totalOutputDuration_(0),
      outputTimePosition_(0),
      inputTimePosition_(0),
      outputTimestamp_(0),
      outputDuration_(0) {
      buffer_ = new AudioBuffer();
    }

    AudioCapture::~AudioCapture() {
      SAFE_RELEASE(deviceEnumerator_);

      SAFE_RELEASE(outputAudioClient_);
      SAFE_RELEASE(outputAudioCaptureClient_);
      SAFE_RELEASE(outputAudioClock_);
      SAFE_RELEASE(outputAudioDevice_);
      outputPwfx_ = nullptr;

      SAFE_RELEASE(inputAudioClient_);
      SAFE_RELEASE(inputAudioCaptureClient_);
      SAFE_RELEASE(inputAudioClock_);
      SAFE_RELEASE(inputAudioDevice_);
      inputPwfx_ = nullptr;

      if (buffer_) {
        delete buffer_;
        buffer_ = nullptr;
      }

      if (file_) {
        mmioClose(file_, 0);
        file_ = nullptr;
      }

      inputAudioSource_ = nullptr;
      outputAudioSource_ = nullptr;

      outputFile_ = nullptr;

      totalOutputNumSamples_ = 0;
      totalOutputBytesLength_ = 0;
      totalOutputDuration_ = 0;

      outputTimestamp_ = 0;
      outputDuration_ = 0;
    }

    FBCAPTURE_STATUS AudioCapture::stopAudioCapture() const {
      if (file_)
        CHECK_MMR_EXIT(mmioClose(file_, 0), L"Failed to close wav output file_.");

      if (outputAudioClient_)
        CHECK_HR_STATUS(outputAudioClient_->Stop(), FBCAPTURE_AUDIO_CAPTURE_STOP_FAILED);

      if (inputAudioClient_)
        CHECK_HR_STATUS(inputAudioClient_->Stop(), FBCAPTURE_AUDIO_CAPTURE_STOP_FAILED);

      return FBCAPTURE_OK;

    exit:
      return FBCAPTURE_AUDIO_CAPTURE_STOP_FAILED;
    }

    HRESULT AudioCapture::findAudioSource(IMMDevice** audioDevice, LPWSTR* audioSource, const bool useRiftAudioSources, EDataFlow flow) const {
      auto hr = S_OK;

      IPropertyStore *pProps = nullptr;
      LPWSTR wstrId; // Device ID.
      PROPVARIANT varName;

      UINT count;
      IMMDeviceCollection *pCollection = nullptr;
      IMMDevice* pickDevice = nullptr;

      if (!useRiftAudioSources) {
        hr = deviceEnumerator_->GetDefaultAudioEndpoint(flow, eConsole, audioDevice);
        CHECK_HR_EXIT(hr);
        hr = (*audioDevice)->GetId(&wstrId);
        CHECK_HR_EXIT(hr);
        hr = (*audioDevice)->OpenPropertyStore(STGM_READ, &pProps);
        CHECK_HR_EXIT(hr);
        PropVariantInit(&varName);
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        CHECK_HR_EXIT(hr);

        DEBUG_LOG_VAR("Found the audio source: ", ConvertToByte(varName.pwszVal));
        *audioSource = varName.pwszVal;

        goto exit;
      }

      CHECK_HR_EXIT(deviceEnumerator_->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &pCollection));
      CHECK_HR_EXIT(pCollection->GetCount(&count));
      if (count == 0) {
        DEBUG_ERROR_VAR("No endpoints found for ", flow == eRender ? "output audio source." : "input audio source.");
        goto exit;
      }

      for (ULONG i = 0; i < count; i++) {
        pCollection->Item(i, &pickDevice);
        pickDevice->GetId(&wstrId);
        pickDevice->OpenPropertyStore(STGM_READ, &pProps);
        PropVariantInit(&varName);
        pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        auto stringAudioSourceName = regex_replace(varName.pwszVal, wregex(L"[0-9]*- "), L"");

        // Finding audio source
        if (stringAudioSourceName.find(RIFT_AUDIO_SOURCE) != std::string::npos) {
          hr = deviceEnumerator_->GetDevice(wstrId, audioDevice);
          CHECK_HR_EXIT(hr);
          *audioSource = varName.pwszVal;
          DEBUG_LOG_VAR("Found the audio source: ", ConvertToByte(*audioSource));
          goto exit;
        }
      }

    exit:

      CoTaskMemFree(wstrId);
      wstrId = nullptr;
      PropVariantClear(&varName);
      SAFE_RELEASE(pProps);
      SAFE_RELEASE(pickDevice);
      SAFE_RELEASE(pCollection);
      SAFE_RELEASE(pickDevice);

      return hr;
    }

    HRESULT AudioCapture::startAudioclient(IMMDevice *audioDevice,
                                           IAudioClient** audioClient,
                                           IAudioCaptureClient** captureClient,
                                           IAudioClock** clockClient,
                                           WAVEFORMATEX **pwfx,
                                           const EDataFlow flow)
    {
      const auto hr = S_OK;
      REFERENCE_TIME devicePeriod;

      /* Activate audio client, audio capture client, and audio clock client */

      CHECK_HR(audioDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)audioClient));
      CHECK_HR((*audioClient)->GetDevicePeriod(&devicePeriod, NULL));
      CHECK_HR((*audioClient)->GetMixFormat(pwfx));

      if ((*pwfx)->wBitsPerSample != 32) {
        DEBUG_ERROR("Unexpected output bit depth, expected 32 bit per samples.");
        return E_FAIL;
      }

      DWORD streamFlags = flow == eRender ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0;

      CHECK_HR((*audioClient)->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags, REFTIMES_PER_SEC, 0, *pwfx, NULL));
      CHECK_HR((*audioClient)->GetService(__uuidof(IAudioCaptureClient), (void**)captureClient));
      CHECK_HR((*audioClient)->GetService(__uuidof(IAudioClock), (void**)clockClient));
      CHECK_HR((*audioClient)->Start());

      return hr;
    }

    FBCAPTURE_STATUS AudioCapture::initialize(const bool mixMic, bool useRiftAudioSources) {
      /* Open WAV output file_ for saving raw audio capture */

//      MMCKINFO ckRiff;
//      MMCKINFO ckData;
//
//      CHECK_HR_STATUS(openFile(outputFile_, &file_), FBCAPTURE_AUDIO_CAPTURE_INIT_FAILED);
//      CHECK_HR_STATUS(writeWaveHeader(file_, outputPwfx_, &ckRiff, &ckData), FBCAPTURE_AUDIO_CAPTURE_INIT_FAILED);

      // Create the device enumerator.
      CHECK_HR_STATUS(CoCreateInstance(
        __uuidof(IMMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&deviceEnumerator_)
      ), FBCAPTURE_AUDIO_CAPTURE_INIT_FAILED);

      CHECK_HR_STATUS(
        findAudioSource(&outputAudioDevice_, &outputAudioSource_, useRiftAudioSources, eRender),
        FBCAPTURE_AUDIO_CAPTURE_INIT_FAILED
      );

      CHECK_HR_STATUS(
        startAudioclient(outputAudioDevice_, &outputAudioClient_, &outputAudioCaptureClient_, &outputAudioClock_, &outputPwfx_, eRender),
        FBCAPTURE_AUDIO_CAPTURE_INIT_FAILED
      );

      // Use default output audio source if output audio source was not found
      if (mixMic) {
        CHECK_HR_STATUS(
          findAudioSource(&inputAudioDevice_, &inputAudioSource_, useRiftAudioSources, eCapture),
          FBCAPTURE_AUDIO_CAPTURE_INIT_FAILED
        );

        CHECK_HR_STATUS(
          startAudioclient(inputAudioDevice_, &inputAudioClient_, &inputAudioCaptureClient_, &inputAudioClock_, &inputPwfx_, eCapture),
          FBCAPTURE_AUDIO_CAPTURE_INIT_FAILED
        );

        // Sanity check for input and output audio mixing
        CHECK_HR_STATUS(inputPwfx_->nSamplesPerSec == outputPwfx_->nSamplesPerSec &&
                        inputPwfx_->wBitsPerSample == outputPwfx_->wBitsPerSample ? S_OK : E_FAIL,
                        FBCAPTURE_AUDIO_CAPTURE_UNSUPPORTED_AUDIO_SOURCE
        );

        mixMic_ = mixMic;
      }

      buffer_->initizalize(mixMic ? Input_Output_Device : Output_Device_Only);
      buffer_->initializeBuffer(BufferIndex_Headphones, outputPwfx_->nChannels);

      if (mixMic)
        buffer_->initializeBuffer(BufferIndex_Microphone, inputPwfx_->nChannels);

      return FBCAPTURE_OK;
    }

    HRESULT AudioCapture::captureAudioFromClient(IAudioCaptureClient* audioCaptureClient,
                                                 IAudioClock* audioClockClient,
                                                 uint8_t** data,
                                                 uint32_t* numFrames,
                                                 LONGLONG* timestamp) const {
      uint32_t nNextPacketSize = 0;
      uint64_t position = 0;
      uint64_t frequency;
      DWORD dwFlags;

      auto hr = audioCaptureClient->GetNextPacketSize(&nNextPacketSize);

      if (!SUCCEEDED(hr) || nNextPacketSize == 0) {
        *numFrames = 0;
        return S_OK;
      }

      CHECK_HR(audioCaptureClient->GetBuffer(data, numFrames, &dwFlags, &position, NULL));

      if (0 == *numFrames)
        CHECK_HR(E_FAIL);

      CHECK_HR(audioClockClient->GetFrequency(&frequency));
      CHECK_HR(audioClockClient->GetPosition(&position, NULL));

      *timestamp = static_cast<LONGLONG>(pow(10, 7) * position / frequency);

      CHECK_HR(audioCaptureClient->ReleaseBuffer(*numFrames));

      return hr;
    }

    FBCAPTURE_STATUS AudioCapture::captureAudio() {
      BYTE *outputData, *inputData;
      uint32_t outputNumFrames = 0, inputNumFrames = 0;

      // Write Output Data
      while (true) {
        CHECK_HR_STATUS(
          captureAudioFromClient(outputAudioCaptureClient_, outputAudioClock_, &outputData, &outputNumFrames, &outputTimePosition_),
          FBCAPTURE_AUDIO_CAPTURE_PACKETS_FAILED
        );

        if (outputNumFrames > 0)
          buffer_->write(BufferIndex_Headphones, reinterpret_cast<float*>(outputData), outputNumFrames);

        if (mixMic_) {
          CHECK_HR_STATUS(
            captureAudioFromClient(inputAudioCaptureClient_, inputAudioClock_, &inputData, &inputNumFrames, &inputTimePosition_),
            FBCAPTURE_AUDIO_CAPTURE_PACKETS_FAILED
          );

          if (inputNumFrames > 0)
            buffer_->write(BufferIndex_Microphone, reinterpret_cast<float*>(inputData), inputNumFrames);
        }

        const auto length = buffer_->getBufferLength();
        if (length > 0)
          break;
      }

      outputDuration_ = outputTimePosition_ - outputTimestamp_;

      return FBCAPTURE_OK;
    }


    FBCAPTURE_STATUS AudioCapture::getOutputBuffer(const float** outputBuffer,
                                                   size_t* numSamples,
                                                   uint64_t* pts,
                                                   uint64_t* duration,
                                                   const bool mute) {
      const auto status = FBCAPTURE_OK;

      if (!buffer_) return FBCAPTURE_AUDIO_CAPTURE_NOT_INITIALIZED;

      buffer_->getBuffer(outputBuffer, numSamples, mute);

      *pts = outputTimestamp_;
      *duration = outputDuration_;

//      Write the current capture output buffer data to output wav file_
//      CHECK_HR_STATUS(writeToFile(buffer_, mute), FBCAPTURE_AUDIO_CAPTURE_NOT_INITIALIZED);

      totalOutputNumSamples_ += static_cast<uint32_t>(*numSamples);
      totalOutputBytesLength_ += static_cast<ULONG>(*numSamples * sizeof(float));
      totalOutputDuration_ += outputDuration_;

      // Update timestamp for the next sample
      outputTimestamp_ = outputTimePosition_;

      return status;
    }

    FBCAPTURE_STATUS AudioCapture::getOutputWavFormat(WAVEFORMATEX **pwfx) const {
      const auto status = FBCAPTURE_OK;
      if (!outputPwfx_) return FBCAPTURE_AUDIO_CAPTURE_NOT_INITIALIZED;
      *pwfx = outputPwfx_;
      return status;
    }

    HRESULT AudioCapture::openFile(const LPCWSTR szFileName, HMMIO *phFile) {
      *phFile = mmioOpen(
        const_cast<LPWSTR>(szFileName),
        nullptr,
        MMIO_READWRITE | MMIO_CREATE | MMIO_DENYNONE
      );

      if (nullptr == *phFile) {
        DEBUG_ERROR("Failed to create audio file_");
        return E_FAIL;
      }

      return S_OK;
    }

    HRESULT AudioCapture::writeWaveHeader(HMMIO file_, WAVEFORMATEX *outputPWFX_, MMCKINFO *pckRiff, MMCKINFO *pckData) {
      pckRiff->ckid = MAKEFOURCC('R', 'I', 'F', 'F');
      pckRiff->fccType = MAKEFOURCC('W', 'A', 'V', 'E');
      pckRiff->cksize = 0;

      CHECK_MMR_EXIT(mmioCreateChunk(file_, pckRiff, MMIO_CREATERIFF), L"Failed to create 'RIFF' chunk.");
      MMCKINFO chunk;

      chunk.ckid = MAKEFOURCC('f', 'm', 't', ' ');
      chunk.cksize = sizeof(PCMWAVEFORMAT);
      CHECK_MMR_EXIT(mmioCreateChunk(file_, &chunk, 0), L"Failed to create 'fmt' chunk.");

      LONG lBytesInWfx = sizeof(WAVEFORMATEX) + outputPWFX_->cbSize;
      LONG lBytesWritten = mmioWrite(file_, reinterpret_cast<PCCH>(const_cast<LPWAVEFORMATEX>(outputPWFX_)), lBytesInWfx);
      CHECK_MMR_EXIT(lBytesWritten == lBytesInWfx ? MMSYSERR_NOERROR : MMSYSERR_ERROR, L"Failed to write WAVEFORMATEX data");

      CHECK_MMR_EXIT(mmioAscend(file_, &chunk, 0), L"Failed to ascend from 'fmt' chunk.");

      chunk.ckid = MAKEFOURCC('f', 'a', 'c', 't');
      CHECK_MMR_EXIT(mmioCreateChunk(file_, &chunk, 0), L"Failed to create a 'fact' chunk.");

      DWORD frames = 0;
      lBytesWritten = mmioWrite(file_, reinterpret_cast<PCCH>(&frames), sizeof(frames));
      CHECK_MMR_EXIT(lBytesWritten == sizeof(frames) ? MMSYSERR_NOERROR : MMSYSERR_ERROR, L"mmioWrite wrote unexpected bytes.");

      CHECK_MMR_EXIT(mmioAscend(file_, &chunk, 0), L"Failed to ascend from 'fmt' chunk.");

      // make a 'data' chunk and leave the data pointer there
      pckData->ckid = MAKEFOURCC('d', 'a', 't', 'a');
      CHECK_MMR_EXIT(mmioCreateChunk(file_, pckData, 0), L"Failed to create 'data' chunk.");

      return S_OK;
    exit:
      return E_FAIL;
    }

    HRESULT AudioCapture::writeToFile(AudioBuffer *buffer, const bool mute) const {
      if (!buffer) goto exit;

      const float* outputBuffer = nullptr;
      size_t outputNumSamples;
      buffer->getBuffer(&outputBuffer, &outputNumSamples, mute);

      CHECK_MMR_EXIT(
        mmioWrite(file_, reinterpret_cast<PCCH>(outputBuffer), static_cast<LONG>(outputNumSamples * sizeof(float))),
        L"Failed to write the capture buffer data to ouput wav file_."
      );

      return S_OK;
    exit:
      return E_FAIL;
    }

    HRESULT AudioCapture::closeWavefile(HMMIO *file, MMCKINFO *pckRiff, MMCKINFO *pckData) {
      CHECK_MMR_EXIT(mmioAscend(*file, pckData, 0), L"Failed to ascend out of a chunk in a RIFF file_.");
      CHECK_MMR_EXIT(mmioAscend(*file, pckRiff, 0), L"Failed to ascend out of a chunk in a Data file_.");
      return S_OK;
    exit:
      return E_FAIL;
    }
  }
}
