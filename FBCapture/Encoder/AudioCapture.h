/****************************************************************************************************************

Filename	:	AudioCapture.h
Content		:
Copyright	:

****************************************************************************************************************/
#pragma once
#define _WINSOCKAPI_
#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <regex>

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
      FBCAPTURE_STATUS stopAudioCapture() const;
      FBCAPTURE_STATUS getOutputBuffer(const float** outputBuffer, size_t* numSamples, uint64_t* pts, uint64_t* duration, bool mute);
      FBCAPTURE_STATUS getOutputWavFormat(WAVEFORMATEX **pwfx) const;

    private:
      HRESULT findAudioSource(IMMDevice** audioDevice,
                              LPWSTR* audioSource,
                              bool useRiftAudioSources,
                              EDataFlow flow) const;

      static HRESULT startAudioclient(IMMDevice *audioDevice,
                                      IAudioClient** audioClient,
                                      IAudioCaptureClient** captureClient,
                                      IAudioClock** clockClient,
                                      WAVEFORMATEX **pwfx,
                                      EDataFlow flow);

      HRESULT captureAudioFromClient(IAudioCaptureClient* audioCaptureClient,
                                     IAudioClock* audioClockClient,
                                     uint8_t** data,
                                     uint32_t* numFrames,
                                     LONGLONG* timestamp) const;

      static HRESULT openFile(LPCWSTR szFileName, HMMIO *phFile);
      static HRESULT writeWaveHeader(HMMIO file, WAVEFORMATEX *outputPwfx, MMCKINFO *pckRiff, MMCKINFO *pckData);
      HRESULT writeToFile(AudioBuffer *buffer, bool mute) const;
      static HRESULT closeWavefile(HMMIO *file, MMCKINFO *pckRiff, MMCKINFO *pckData);

    private:
      IMMDeviceEnumerator *deviceEnumerator_;

      LPWSTR inputAudioSource_;
      IAudioClient* outputAudioClient_;
      IAudioCaptureClient* outputAudioCaptureClient_;
      IAudioClock* outputAudioClock_;
      IMMDevice *outputAudioDevice_;
      WAVEFORMATEX *outputPwfx_;

      LPWSTR outputAudioSource_;
      IAudioClient* inputAudioClient_;
      IAudioCaptureClient* inputAudioCaptureClient_;
      IAudioClock* inputAudioClock_;
      IMMDevice* inputAudioDevice_;
      WAVEFORMATEX *inputPwfx_;

      AudioBuffer* buffer_;
      HMMIO file_;
      LPCWSTR outputFile_;

      bool mixMic_;

      uint32_t totalOutputNumSamples_;
      ULONG totalOutputBytesLength_;
      uint64_t totalOutputDuration_;         // 100-nanosecond unit

      LONGLONG outputTimePosition_;
      LONGLONG inputTimePosition_;

      uint64_t outputTimestamp_;
      uint64_t outputDuration_;
    };

  }
}
