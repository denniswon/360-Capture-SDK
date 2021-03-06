#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits>
#include <assert.h>

#include "AudioBuffer.h"

#define NOMINMAX
#define MAX_AV_PLANES 8

namespace FBCapture {
  namespace Audio {

    AudioBuffer::~AudioBuffer() {
      for (auto i = 0; i < numBuffers_; ++i) {
        auto& buff = buffers_[i];
        if (buff.data_ != nullptr) {
          free(buff.data_);
        }
      }

      if (mixBuffer_ != nullptr) {
        free(mixBuffer_);
      }
    }

    void AudioBuffer::initizalize(const int numBuffers) {
      numBuffers_ = numBuffers;
      buffers_ = new Buffer[numBuffers];
      memset(buffers_, 0, numBuffers * sizeof(Buffer));
      mixBuffer_ = static_cast<float*>(malloc(kMIX_BUFFER_LENGTH * kSTEREO * sizeof(float)));
    }

    void AudioBuffer::initializeBuffer(const int index, const int channelCount) const {
      if (index >= numBuffers_) {
        return; // out of bounds!
      }

      Buffer& buff = buffers_[index];

      buff.channelCount_ = channelCount;
    }

    void AudioBuffer::write(const int index, const float* data, const size_t lengthFrames) const {
      if (index >= numBuffers_) {
        return; // out of bounds!
      }

      auto& buff = buffers_[index];
      assert(buff.positionFrames_ >= 0);

      const auto requiredLengthFrames = buff.positionFrames_ + static_cast<int>(lengthFrames);

      if (requiredLengthFrames > (1024 * 500)) {
        return; // sanity check!
      }

      if (buff.length_ < requiredLengthFrames) {
        buff.data_ = static_cast<float*>(realloc(buff.data_, requiredLengthFrames * buff.channelCount_ * sizeof(float)));
        buff.length_ = requiredLengthFrames;
      }

      memcpy(buff.data_ + buff.positionFrames_ * buff.channelCount_, data, lengthFrames * buff.channelCount_ * sizeof(float));
      buff.positionFrames_ += static_cast<int>(lengthFrames);
      assert(buff.positionFrames_ >= 0);
    }

    size_t AudioBuffer::getBufferLength() const {
      int len = kMIX_BUFFER_LENGTH;

      for (auto i = 0; i < numBuffers_; ++i) {
        if (buffers_[i].positionFrames_ < len) {
          assert(buffers_[i].positionFrames_ >= 0);
          len = buffers_[i].positionFrames_;
        }
      }

      return len * kSTEREO;
    }

    void AudioBuffer::getBuffer(const float** buffer, size_t* length, const bool silenceMode) const {
      int len = kMIX_BUFFER_LENGTH;

      for (auto i = 0; i < numBuffers_; ++i) {
        if (buffers_[i].positionFrames_ < len) {
          assert(buffers_[i].positionFrames_ >= 0);
          len = buffers_[i].positionFrames_;
        }
      }

      if (len <= 0)
        return;

      memset(mixBuffer_, 0, kMIX_BUFFER_LENGTH * sizeof(float));

      for (auto i = 0; i < numBuffers_; ++i) {
        auto& buff = buffers_[i];
        for (auto frame = 0; frame < len; ++frame) {
          for (auto chan = 0; chan < kSTEREO; ++chan) {
            if (!silenceMode) {
              // read the most recent frames
              const auto frameIdx = buff.positionFrames_ - len + frame;
              mixBuffer_[(frame * kSTEREO) + chan] += buff.data_[(frameIdx * buff.channelCount_) + chan % buff.channelCount_];
            }
          }
        }

        // Compact the buffer
        memmove(buff.data_, buff.data_ + (len  * buff.channelCount_), (buff.length_ - len) * sizeof(float) * buff.channelCount_);
        buff.positionFrames_ -= len;
        assert(buff.positionFrames_ >= 0);
      }

      *length = len * kSTEREO;
      *buffer = mixBuffer_;
    }

    void AudioBuffer::convertTo16Bit(const short **outputBuffer, const float *buffer, const size_t length) {
      const auto buf = static_cast<short*>(malloc(length * sizeof(short)));

      for (auto i = 0; i < length; i++) {
        buf[i] = static_cast<short>(buffer[i] * 32768);
      }

      *outputBuffer = buf;
    }
  }
}
