/****************************************************************************************************************

Filename	:	MFAudioEncoder.h
Content		:
Copyright	:

****************************************************************************************************************/

#pragma once
#define _WINSOCKAPI_
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mftransform.h>
#include <iostream>

#include "EncodePacket.h"
#include "FBCaptureStatus.h"

using namespace std;
using namespace FBCapture::Streaming;

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

#define AAC_PAYLOAD_TYPE 0 /* raw aac block */
#define STREAM_INDEX 0 /* raw aac block */

namespace FBCapture {
  namespace Audio {

    /* informs the client if the encoded packet output is available or not */
    enum EncStatus {
      ENC_FAILURE,
      ENC_SUCCESS,
      ENC_NOT_ACCEPTING,
      ENC_NEED_MORE_INPUT
    };

    class MFAudioEncoder {

    public:
      MFAudioEncoder();
      virtual ~MFAudioEncoder();

      /* initializes the AAC encoder that can either encode wav input file_ or wav input stream packets as aac file_/packet output */
      FBCAPTURE_STATUS initialize(WAVEFORMATEX *wavPWFX, const wchar_t* dstFile);

      /* encode input wav audio file_ into aac encoded output file_ */
      FBCAPTURE_STATUS encodeFile(const wstring srcFile, const wstring dstFile);

      /* encode raw wav audio packets as stream of data */
      FBCAPTURE_STATUS encode(const float* buffer, uint32_t numSamples, LONGLONG pts, uint64_t duration, EncStatus *encStatus);

      /*
       * if encodePacket() returns EncStatus ENC_SUCCESS, getOutputBuffer() returns the pointer to the encoded output buffer data
       * outputLength in uint8_t length, outputPts in 100-nanosecond units, outputDuration in 100-nanosecond units.
       */
      FBCAPTURE_STATUS getOutputBuffer(uint8_t** buffer, DWORD *length, LONGLONG* pts, LONGLONG* duration);

      /* finalizes encodePacket() session and closes the final aac output file_ */
      FBCAPTURE_STATUS finalize();

      /* gets AAC sequence header data for the encoding session */
      FBCAPTURE_STATUS getSequenceParams(uint32_t* profile_Level, uint32_t* sampleRate, uint32_t* numChannels) const;

      FBCAPTURE_STATUS getEncodePacket(AudioEncodePacket** packet);

    private:

      HRESULT addSinkWriter(const wchar_t* dstFile);

      /* private helper functions used for encodeFile() */

      HRESULT addSourceNode(IMFTopology* topology, IMFPresentationDescriptor* presDesc, IMFStreamDescriptor* streamDesc, IMFTopologyNode** node) const;
      HRESULT addTransformNode(IMFTopology* topology, IMFTopologyNode* srcNode, IMFStreamDescriptor* streamDesc, IMFTopologyNode** node);
      HRESULT addOutputNode(IMFTopology* topology, IMFTopologyNode* transformNode, IMFTopologyNode** outputNode, const wstring dstFile);
      HRESULT handleMediaEvent(IMFMediaEvent **mediaSessionEvent) const;

      /* private helper functions used for encodePacket() */

      HRESULT setTransformInputType(IMFTransform** transform);
      HRESULT setTransformOutputType(IMFTransform** transform);
      HRESULT processInput(const float* buffer, uint32_t numFrames, LONGLONG pts, uint64_t duration);
      HRESULT processOutput();

      HRESULT shutdownSessions();

    private:
      WAVEFORMATEX *inWavFormat_;
      WAVEFORMATEX *outWavFormat_;
      HEAACWAVEFORMAT* outAacWavFormat_;

      /* private class members for encodeFile() */

      IMFMediaSession *mediaSession_;
      IMFMediaSource *mediaSource_;
      IMFMediaSink *sink_;

      IMFSourceResolver *sourceResolver_;
      IMFTopology *topology;
      IMFPresentationDescriptor *presDESC;
      IMFStreamDescriptor *streamDESC;
      IMFTopologyNode *srcNode_;
      IMFTopologyNode *transformNode_;
      IMFTopologyNode *outputNode_;

      /* private class members for encodePacket() */

      IMFTransform *transform_;
      IMFMediaType *inputMediaType_;
      IMFMediaType *outputMediaType_;

      DWORD streamIndex_;
      IMFByteStream *outputByteStream_;
      IMFStreamSink *outputStreamSink_;
      IMFMediaSink *outputMediaSink_;
      IMFSinkWriter *outputSinkWriter_;

      IMFSample* inputSample_;
      IMFMediaBuffer* inputBuffer_;

      IMFSample *outputSample_;
      IMFMediaBuffer* outputBuffer_;

      uint8_t *outputBufferData_;
      DWORD outputBufferLength_;
      LONGLONG outputSamplePts_;
      LONGLONG outputSampleDuration_;

    public:
      static const uint32_t kProfileLevel;
      static const MFT_REGISTER_TYPE_INFO kInInfo;
      static const MFT_REGISTER_TYPE_INFO kOutInfo;
    };
  }
}