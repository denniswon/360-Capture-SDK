/****************************************************************************************************************

Filename	:	AudioCapture.h
Content		:
Copyright	:

****************************************************************************************************************/
#pragma once
#define _WINSOCKAPI_
#include <Windows.h>
#include <chrono>
#include <iostream>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <iostream>
#include <initguid.h>
#include <stdio.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <avrt.h>
#include <regex>
#include <functiondiscoverykeys_devpkey.h>

#define RIFT_AUDIO_SOURCE L"Rift Audio"

#include "FBCaptureStatus.h"
#include "AudioBuffer.h"

using namespace std;

enum BufferIndex {
  BufferIndex_Headphones = 0,
  BufferIndex_Microphone,
  BufferIndex_Max
};

enum BufferNums {
  No_Device = 0,
  Output_Device_Only,
  Input_Output_Device
};

namespace FBCapture {
  namespace Audio {

    class AudioCapture {
    public:
      AudioCapture();
      virtual ~AudioCapture();

      FBCAPTURE_STATUS initialize(bool mixMic, bool useRiftAudioSources);
      FBCAPTURE_STATUS captureAudio();
      FBCAPTURE_STATUS stopAudioCapture();
      FBCAPTURE_STATUS getOutputBuffer(const float** outputBuffer, size_t* numSamples, uint64_t* pts, uint64_t* duration, bool mute);
      FBCAPTURE_STATUS getOutputWavFormat(WAVEFORMATEX **pwfx);

    private:
      HRESULT findAudioSource(IMMDevice** audioDevice, LPWSTR* audioSource, bool useRiftAudioSources, EDataFlow flow);
      HRESULT startAudioclient(IMMDevice *audioDevice, IAudioClient** audioClient, IAudioCaptureClient** captureClient, IAudioClock** clockClient, WAVEFORMATEX **pwfx, EDataFlow flow);
      HRESULT captureAudioFromClient(IAudioCaptureClient* audioCaptureClient, IAudioClock* audioClockClient, uint8_t** data, uint32_t* numFrames, LONGLONG* timestamp);

      HRESULT openFile(LPCWSTR szFileName, HMMIO *phFile);
      HRESULT writeWaveHeader(HMMIO file, WAVEFORMATEX *outputPWFX, MMCKINFO *pckRiff, MMCKINFO *pckData);
      HRESULT writeToFile(AudioBuffer *buffer, bool mute);
      HRESULT closeWavefile(HMMIO *file, MMCKINFO *pckRiff, MMCKINFO *pckData);

    private:
      IMMDeviceEnumerator *deviceEnumerator;

      LPWSTR inputAudioSource;
      IAudioClient* outputAudioClient;
      IAudioCaptureClient* outputAudioCaptureClient;
      IAudioClock* outputAudioClock;
      IMMDevice *outputAudioDevice;
      WAVEFORMATEX *outputPWFX;

      LPWSTR outputAudioSource;
      IAudioClient* inputAudioClient;
      IAudioCaptureClient* inputAudioCaptureClient;
      IAudioClock* inputAudioClock;
      IMMDevice* inputAudioDevice;
      WAVEFORMATEX *inputPWFX;

      AudioBuffer* buffer;
      HMMIO file;
      LPCWSTR outputFile;

      bool mixMic_;

      uint32_t totalOutputNumSamples;
      ULONG totalOutputBytesLength;
      uint64_t totalOutputDuration;         // 100-nanosecond unit

      LONGLONG outputTimePosition;
      LONGLONG inputTimePosition;

      uint64_t outputTimestamp;
      uint64_t outputDuration;
    };

  }
}
