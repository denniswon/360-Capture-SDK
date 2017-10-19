/****************************************************************************************************************

Filename	:	AudioBuffer.h
Content		:	A adaptive resampling ringbuffer to sync to input signals at different and varying sample rates
Created		:	June 16, 2017
Authors		:	Pete Stirling

Copyright	:

****************************************************************************************************************/

#pragma once

#include <Audioclient.h>

namespace FBCapture {
  namespace Audio {

    class AudioBuffer {
    public:
      AudioBuffer() {}
      ~AudioBuffer();

      void initizalize(int numBuffers);
      void initializeBuffer(int index, int channelCount); // WAVEFORMATEX *bufferFormat, IAudioClock* clock);
      void write(int index, const float* data, size_t length);
      void getBuffer(const float** buffer, size_t* length, bool silenceMode);
      size_t getBufferLength();

      static void convertTo16Bit(const short **outputBuffer, const float *buffer, size_t length);

      static const size_t kMIX_BUFFER_LENGTH = 4096; // PAS
      static const int kSTEREO = 2;

    private:
      struct Buffer {
        float* data_;
        int channelCount_;
        int length_;
        int positionFrames_;
      };

      Buffer* buffers_;
      int numBuffers_;
      float* mixBuffer_;
    };

  }
}
