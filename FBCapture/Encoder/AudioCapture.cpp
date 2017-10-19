/****************************************************************************************************************

Filename	:	AudioCapture.cpp
Content		:
Copyright	:

****************************************************************************************************************/

#include "AudioCapture.h"
#include "Common.h"
#include "Log.h"

#define REFTIMES_PER_SEC  10000000

namespace FBCapture {
  namespace Audio {

    AudioCapture::AudioCapture() {
      deviceEnumerator = NULL;

      outputAudioSource = NULL;
      outputAudioClient = NULL;
      outputAudioCaptureClient = NULL;
      outputAudioClock = NULL;
      outputAudioDevice = NULL;
      outputPWFX = NULL;

      inputAudioSource = NULL;
      inputAudioClient = NULL;
      inputAudioCaptureClient = NULL;
      inputAudioClock = NULL;
      inputAudioDevice = NULL;
      inputPWFX = NULL;

      buffer = new AudioBuffer();
      file = NULL;
      outputFile = NULL;
      mixMic_ = false;

      totalOutputNumSamples = 0;
      totalOutputBytesLength = 0;
      totalOutputDuration = 0;

      outputTimestamp = 0;
      outputDuration = 0;
    }

    AudioCapture::~AudioCapture() {
      SAFE_RELEASE(deviceEnumerator);

      SAFE_RELEASE(outputAudioClient);
      SAFE_RELEASE(outputAudioCaptureClient);
      SAFE_RELEASE(outputAudioClock);
      SAFE_RELEASE(outputAudioDevice);
      outputPWFX = NULL;

      SAFE_RELEASE(inputAudioClient);
      SAFE_RELEASE(inputAudioCaptureClient);
      SAFE_RELEASE(inputAudioClock);
      SAFE_RELEASE(inputAudioDevice);
      inputPWFX = NULL;

      if (buffer) {
        delete buffer;
        buffer = NULL;
      }

      if (file) {
        mmioClose(file, 0);
        file = NULL;
      }

      inputAudioSource = NULL;
      outputAudioSource = NULL;

      outputFile = NULL;

      totalOutputNumSamples = 0;
      totalOutputBytesLength = 0;
      totalOutputDuration = 0;

      outputTimestamp = 0;
      outputDuration = 0;
    }

    FBCAPTURE_STATUS AudioCapture::stopAudioCapture() {
      if (file)
        CHECK_MMR_EXIT(mmioClose(file, 0), L"Failed to close wav output file.");

      if (outputAudioClient)
        CHECK_HR_STATUS(outputAudioClient->Stop(), FBCAPTURE_AUDIO_CAPTURE_STOP_FAILED);

      if (inputAudioClient)
        CHECK_HR_STATUS(inputAudioClient->Stop(), FBCAPTURE_AUDIO_CAPTURE_STOP_FAILED);

      return FBCAPTURE_OK;

    exit:
      return FBCAPTURE_AUDIO_CAPTURE_STOP_FAILED;
    }

    HRESULT AudioCapture::findAudioSource(IMMDevice** audioDevice, LPWSTR* audioSource, bool useRiftAudioSources, EDataFlow flow) {
      HRESULT hr = S_OK;

      IPropertyStore *pProps = NULL;
      LPWSTR wstrID; // Device ID.
      PROPVARIANT varName;

      UINT count;
      IMMDeviceCollection *pCollection = NULL;
      IMMDevice* pickDevice = NULL;

      if (!useRiftAudioSources) {
        hr = deviceEnumerator->GetDefaultAudioEndpoint(flow, eConsole, audioDevice);
        CHECK_HR_EXIT(hr);
        hr = (*audioDevice)->GetId(&wstrID);
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

      CHECK_HR_EXIT(deviceEnumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &pCollection));
      CHECK_HR_EXIT(pCollection->GetCount(&count));
      if (count == 0) {
        DEBUG_ERROR_VAR("No endpoints found for ", flow == eRender ? "output audio source." : "input audio source.");
        goto exit;
      }

      for (ULONG i = 0; i < count; i++) {
        pCollection->Item(i, &pickDevice);
        pickDevice->GetId(&wstrID);
        pickDevice->OpenPropertyStore(STGM_READ, &pProps);
        PropVariantInit(&varName);
        pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        wstring stringAudioSourceName = regex_replace(varName.pwszVal, wregex(L"[0-9]*- "), L"");

        // Finding audio source
        if (stringAudioSourceName.find(RIFT_AUDIO_SOURCE) != std::string::npos) {
          hr = deviceEnumerator->GetDevice(wstrID, audioDevice);
          CHECK_HR_EXIT(hr);
          *audioSource = varName.pwszVal;
          DEBUG_LOG_VAR("Found the audio source: ", ConvertToByte(*audioSource));
          goto exit;
        }
      }

    exit:

      CoTaskMemFree(wstrID);
      wstrID = NULL;
      PropVariantClear(&varName);
      SAFE_RELEASE(pProps);
      SAFE_RELEASE(pickDevice);
      SAFE_RELEASE(pCollection);
      SAFE_RELEASE(pickDevice);

      return hr;
    }

    HRESULT AudioCapture::startAudioclient(IMMDevice *audioDevice, IAudioClient** audioClient, IAudioCaptureClient** captureClient, IAudioClock** clockClient, WAVEFORMATEX **pwfx, EDataFlow flow) {
      HRESULT hr = S_OK;
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

    FBCAPTURE_STATUS AudioCapture::initialize(bool mixMic, bool useRiftAudioSources) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      /* Open WAV output file for saving raw audio capture */

      /*MMCKINFO ckRIFF;
      MMCKINFO ckData;

      CHECK_HR_STATUS(openFile(outputFile, &file), FBCAPTURE_AUDIO_CAPTURE_INIT_FAILED);
      CHECK_HR_STATUS(writeWaveHeader(file, outputPWFX, &ckRIFF, &ckData), FBCAPTURE_AUDIO_CAPTURE_INIT_FAILED);*/

      // Create the device enumerator.
      CHECK_HR_STATUS(CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&deviceEnumerator
      ), FBCAPTURE_AUDIO_CAPTURE_INIT_FAILED);

      CHECK_HR_STATUS(
        findAudioSource(&outputAudioDevice, &outputAudioSource, useRiftAudioSources, eRender),
        FBCAPTURE_AUDIO_CAPTURE_INIT_FAILED
      );

      CHECK_HR_STATUS(
        startAudioclient(outputAudioDevice, &outputAudioClient, &outputAudioCaptureClient, &outputAudioClock, &outputPWFX, eRender),
        FBCAPTURE_AUDIO_CAPTURE_INIT_FAILED
      );

      // Use default output audio source if output audio source was not found
      if (mixMic) {
        CHECK_HR_STATUS(
          findAudioSource(&inputAudioDevice, &inputAudioSource, useRiftAudioSources, eCapture),
          FBCAPTURE_AUDIO_CAPTURE_INIT_FAILED
        );

        CHECK_HR_STATUS(
          startAudioclient(inputAudioDevice, &inputAudioClient, &inputAudioCaptureClient, &inputAudioClock, &inputPWFX, eCapture),
          FBCAPTURE_AUDIO_CAPTURE_INIT_FAILED
        );

        // Sanity check for input and output audio mixing
        CHECK_HR_STATUS(inputPWFX->nSamplesPerSec == outputPWFX->nSamplesPerSec &&
                        inputPWFX->wBitsPerSample == outputPWFX->wBitsPerSample ? S_OK : E_FAIL,
                        FBCAPTURE_AUDIO_CAPTURE_UNSUPPORTED_AUDIO_SOURCE
        );

        mixMic_ = mixMic;
      }

      buffer->initizalize(mixMic ? Input_Output_Device : Output_Device_Only);
      buffer->initializeBuffer(BufferIndex_Headphones, outputPWFX->nChannels);

      if (mixMic)
        buffer->initializeBuffer(BufferIndex_Microphone, inputPWFX->nChannels);

      return FBCAPTURE_OK;
    }

    HRESULT AudioCapture::captureAudioFromClient(IAudioCaptureClient* audioCaptureClient, IAudioClock* audioClockClient, uint8_t** data, uint32_t* numFrames, LONGLONG* timestamp) {
      HRESULT hr = S_OK;

      uint32_t nNextPacketSize = 0;
      uint64_t position = 0;
      uint64_t frequency;
      DWORD dwFlags;

      hr = audioCaptureClient->GetNextPacketSize(&nNextPacketSize);

      if (!SUCCEEDED(hr) || nNextPacketSize == 0) {
        *numFrames = 0;
        return S_OK;
      }

      CHECK_HR(audioCaptureClient->GetBuffer(data, numFrames, &dwFlags, &position, NULL));

      if (0 == *numFrames)
        CHECK_HR(E_FAIL);

      CHECK_HR(audioClockClient->GetFrequency(&frequency));
      CHECK_HR(audioClockClient->GetPosition(&position, NULL));

      *timestamp = (LONGLONG)(pow(10, 7) * position / frequency);

      CHECK_HR(audioCaptureClient->ReleaseBuffer(*numFrames));

      return hr;
    }

    FBCAPTURE_STATUS AudioCapture::captureAudio() {
      HRESULT hr = S_OK;
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      BYTE *outputData, *inputData;
      uint32_t outputNumFrames = 0, inputNumFrames = 0;

      // Write Output Data
      while (true) {
        CHECK_HR_STATUS(
          captureAudioFromClient(outputAudioCaptureClient, outputAudioClock, &outputData, &outputNumFrames, &outputTimePosition),
          FBCAPTURE_AUDIO_CAPTURE_PACKETS_FAILED
        );

        if (outputNumFrames > 0)
          buffer->write(BufferIndex_Headphones, reinterpret_cast<float*>(outputData), outputNumFrames);

        if (mixMic_) {
          CHECK_HR_STATUS(
            captureAudioFromClient(inputAudioCaptureClient, inputAudioClock, &inputData, &inputNumFrames, &inputTimePosition),
            FBCAPTURE_AUDIO_CAPTURE_PACKETS_FAILED
          );

          if (inputNumFrames > 0)
            buffer->write(BufferIndex_Microphone, reinterpret_cast<float*>(inputData), inputNumFrames);
        }

        size_t length = buffer->getBufferLength();
        if (length > 0)
          break;
      }

      outputDuration = outputTimePosition - outputTimestamp;

      return FBCAPTURE_OK;
    }


    FBCAPTURE_STATUS AudioCapture::getOutputBuffer(const float** outputBuffer, size_t* numSamples, uint64_t* pts, uint64_t* duration, bool mute) {
      HRESULT hr = S_OK;
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      if (!buffer) return FBCAPTURE_AUDIO_CAPTURE_NOT_INITIALIZED;

      buffer->getBuffer(outputBuffer, numSamples, mute);

      *pts = outputTimestamp;
      *duration = outputDuration;

      // Write the current capture output buffer data to output wav file
      // CHECK_HR_STATUS(writeToFile(buffer, mute), FBCAPTURE_AUDIO_CAPTURE_NOT_INITIALIZED);

      totalOutputNumSamples += (uint32_t)(*numSamples);
      totalOutputBytesLength += (ULONG)(*numSamples * sizeof(float));
      totalOutputDuration += outputDuration;

      // Update timestamp for the next sample
      outputTimestamp = outputTimePosition;

      return status;
    }

    FBCAPTURE_STATUS AudioCapture::getOutputWavFormat(WAVEFORMATEX **pwfx) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;
      if (!outputPWFX) return FBCAPTURE_AUDIO_CAPTURE_NOT_INITIALIZED;
      *pwfx = outputPWFX;
      return status;
    }

    HRESULT AudioCapture::openFile(LPCWSTR szFileName, HMMIO *phFile) {
      *phFile = mmioOpen(
        const_cast<LPWSTR>(szFileName),
        NULL,
        MMIO_READWRITE | MMIO_CREATE | MMIO_DENYNONE
      );

      if (NULL == *phFile) {
        DEBUG_ERROR("Failed to create audio file");
        return E_FAIL;
      }

      return S_OK;
    }

    HRESULT AudioCapture::writeWaveHeader(HMMIO file, WAVEFORMATEX *outputPWFX, MMCKINFO *pckRiff, MMCKINFO *pckData) {
      pckRiff->ckid = MAKEFOURCC('R', 'I', 'F', 'F');
      pckRiff->fccType = MAKEFOURCC('W', 'A', 'V', 'E');
      pckRiff->cksize = 0;

      CHECK_MMR_EXIT(mmioCreateChunk(file, pckRiff, MMIO_CREATERIFF), L"Failed to create 'RIFF' chunk.");
      MMCKINFO chunk;

      chunk.ckid = MAKEFOURCC('f', 'm', 't', ' ');
      chunk.cksize = sizeof(PCMWAVEFORMAT);
      CHECK_MMR_EXIT(mmioCreateChunk(file, &chunk, 0), L"Failed to create 'fmt' chunk.");

      LONG lBytesInWfx = sizeof(WAVEFORMATEX) + outputPWFX->cbSize;
      LONG lBytesWritten = mmioWrite(file, reinterpret_cast<PCCH>(const_cast<LPWAVEFORMATEX>(outputPWFX)), lBytesInWfx);
      CHECK_MMR_EXIT(lBytesWritten == lBytesInWfx ? MMSYSERR_NOERROR : MMSYSERR_ERROR, L"Failed to write WAVEFORMATEX data");

      CHECK_MMR_EXIT(mmioAscend(file, &chunk, 0), L"Failed to ascend from 'fmt' chunk.");

      chunk.ckid = MAKEFOURCC('f', 'a', 'c', 't');
      CHECK_MMR_EXIT(mmioCreateChunk(file, &chunk, 0), L"Failed to create a 'fact' chunk.");

      DWORD frames = 0;
      lBytesWritten = mmioWrite(file, reinterpret_cast<PCCH>(&frames), sizeof(frames));
      CHECK_MMR_EXIT(lBytesWritten == sizeof(frames) ? MMSYSERR_NOERROR : MMSYSERR_ERROR, L"mmioWrite wrote unexpected bytes.");

      CHECK_MMR_EXIT(mmioAscend(file, &chunk, 0), L"Failed to ascend from 'fmt' chunk.");

      // make a 'data' chunk and leave the data pointer there
      pckData->ckid = MAKEFOURCC('d', 'a', 't', 'a');
      CHECK_MMR_EXIT(mmioCreateChunk(file, pckData, 0), L"Failed to create 'data' chunk.");

      return S_OK;
    exit:
      return E_FAIL;
    }

    HRESULT AudioCapture::writeToFile(AudioBuffer *buffer, bool mute) {
      if (!buffer) goto exit;

      const float* outputBuffer = nullptr;
      size_t outputNumSamples;
      buffer->getBuffer(&outputBuffer, &outputNumSamples, mute);

      CHECK_MMR_EXIT(
        mmioWrite(file, reinterpret_cast<PCCH>(outputBuffer), (LONG)(outputNumSamples * sizeof(float))),
        L"Failed to write the capture buffer data to ouput wav file."
      );

      return S_OK;
    exit:
      return E_FAIL;
    }

    HRESULT AudioCapture::closeWavefile(HMMIO *file, MMCKINFO *pckRiff, MMCKINFO *pckData) {
      CHECK_MMR_EXIT(mmioAscend(*file, pckData, 0), L"Failed to ascend out of a chunk in a RIFF file.");
      CHECK_MMR_EXIT(mmioAscend(*file, pckRiff, 0), L"Failed to ascend out of a chunk in a Data file.");
      return S_OK;
    exit:
      return E_FAIL;
    }
  }
}
