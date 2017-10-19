#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

  namespace FBCapture {

    struct FBCaptureConfig {
      FBCaptureConfig() :
        bitrate(0),
        fps(0),
        gop(0),
        flipTexture(false),
        mute(false),
        mixMic(false),
        useRiftAudioSources(false),
        enableAsyncMode(false) {}

      // Encoding option [required]
      uint32_t bitrate;
      uint32_t fps;
      uint32_t gop;

      // Capture texture option [optional]
      bool flipTexture;

      // Audio capture option [optional]
      bool mute;
      bool mixMic;
      bool useRiftAudioSources;

      // Enable async encoding mode for non-blocking FBCaptureEncodeFrame() call [optional]
      bool enableAsyncMode;
    };
  }

#ifdef __cplusplus
}
#endif