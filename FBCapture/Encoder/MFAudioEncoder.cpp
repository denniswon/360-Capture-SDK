/****************************************************************************************************************

Filename	:	MFAudioEncoder.cpp
Content		:
Copyright	:

****************************************************************************************************************/

#include "MFAudioEncoder.h"
#include "AudioBuffer.h"
#include "Common.h"
#include "Log.h"
#include <mferror.h>
#include <wmcodecdsp.h>

namespace FBCapture {
  namespace Audio {

    const uint32_t MFAudioEncoder::kProfileLevel = 0x2;

    const MFT_REGISTER_TYPE_INFO MFAudioEncoder::kInInfo = { MFMediaType_Audio, MFAudioFormat_PCM };
    const MFT_REGISTER_TYPE_INFO MFAudioEncoder::kOutInfo = { MFMediaType_Audio, MFAudioFormat_AAC };

    MFAudioEncoder::MFAudioEncoder() :
      inWavFormat_(NULL),
      outWavFormat_(NULL),
      outAacWavFormat_(NULL),
      mediaSession_(NULL),
      mediaSource_(NULL),
      sink_(NULL),
      sourceResolver_(NULL),
      topology(NULL),
      presDESC(NULL),
      streamDESC(NULL),
      srcNode_(NULL),
      transformNode_(NULL),
      outputNode_(NULL),
      transform_(NULL),
      inputMediaType_(NULL),
      outputMediaType_(NULL),
      streamIndex_(STREAM_INDEX),
      outputByteStream_(NULL),
      outputStreamSink_(NULL),
      outputMediaSink_(NULL),
      outputSinkWriter_(NULL),
      inputSample_(NULL),
      inputBuffer_(NULL),
      outputSample_(NULL),
      outputBuffer_(NULL),
      outputBufferData_(NULL),
      outputBufferLength_(0),
      outputSamplePts_(0),
      outputSampleDuration_(0) {}

    MFAudioEncoder::~MFAudioEncoder() {
      finalize();
      shutdownSessions();
    }

    HRESULT MFAudioEncoder::setTransformInputType(IMFTransform** transform) {
      DWORD dwTypeIndex = 0;
      IMFMediaType *ppType;

      GUID mediaMajorType;
      GUID mediaSubType;
      uint32_t bitDepth;
      uint32_t samplesPerSec;
      uint32_t nChannels;

      CHECK_HR(MFCreateMediaType(&inputMediaType_));

      while (true) {
        auto hr = (*transform)->GetInputAvailableType(0, dwTypeIndex, &ppType);
        if (hr == MF_E_NO_MORE_TYPES) break;

        ppType->GetMajorType(&mediaMajorType);
        ppType->GetGUID(MF_MT_SUBTYPE, &mediaSubType);
        ppType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitDepth);
        ppType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &samplesPerSec);
        ppType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &nChannels);

        if (mediaMajorType == MFMediaType_Audio &&
            mediaSubType == MFAudioFormat_PCM &&
            // MF AAC Encoder only allows 16 bit depth input sample media types
            bitDepth == 16 &&
            samplesPerSec == inWavFormat_->nSamplesPerSec &&
            nChannels == AudioBuffer::kSTEREO) {

          CHECK_HR(inputMediaType_->SetGUID(MF_MT_MAJOR_TYPE, mediaMajorType));
          CHECK_HR(inputMediaType_->SetGUID(MF_MT_SUBTYPE, mediaSubType));
          CHECK_HR(inputMediaType_->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitDepth));
          CHECK_HR(inputMediaType_->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, samplesPerSec));
          CHECK_HR(inputMediaType_->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, nChannels));

          hr = (*transform)->SetInputType(streamIndex_, inputMediaType_, NULL);
          if (SUCCEEDED(hr))
            break;
          else
            inputMediaType_->DeleteAllItems();
        }

        dwTypeIndex++;
      }

      return S_OK;
    }

    HRESULT MFAudioEncoder::setTransformOutputType(IMFTransform** transform) {
      DWORD dwTypeIndex = 0;
      IMFMediaType *ppType;

      GUID mediaMajorType;
      GUID mediaSubType;
      uint32_t payloadType;
      uint32_t nChannels;
      uint32_t avgBytePerSecond;
      uint32_t bitDepth;
      uint32_t samplesPerSec;
      uint32_t profile_Level;
      uint32_t blockAlign;

      uint32_t inputBitDepth;
      uint32_t inputSampleRate;
      uint32_t inputNumChannels;

      /* Output type requirement for MF AAC Encoder https://msdn.microsoft.com/en-us/library/windows/desktop/dd742785(v=vs.85).aspx */
      inputMediaType_->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &inputBitDepth);
      inputMediaType_->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &inputSampleRate);
      inputMediaType_->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &inputNumChannels);

      CHECK_HR(MFCreateMediaType(&outputMediaType_));

      while (true) {
        auto hr = (*transform)->GetOutputAvailableType(0, dwTypeIndex, &ppType);
        if (hr == MF_E_NO_MORE_TYPES) break;

        ppType->GetMajorType(&mediaMajorType);
        ppType->GetGUID(MF_MT_SUBTYPE, &mediaSubType);
        ppType->GetUINT32(MF_MT_AAC_PAYLOAD_TYPE, &payloadType);
        ppType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &nChannels);
        ppType->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &avgBytePerSecond);
        ppType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitDepth);
        ppType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &samplesPerSec);
        ppType->GetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, &profile_Level);
        ppType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlign);

        if (mediaMajorType == MFMediaType_Audio &&
            mediaSubType == MFAudioFormat_AAC &&
            bitDepth == inputBitDepth &&
            samplesPerSec == inputSampleRate &&
            nChannels == inputNumChannels &&
            payloadType == AAC_PAYLOAD_TYPE) {

          CHECK_HR(outputMediaType_->SetGUID(MF_MT_MAJOR_TYPE, mediaMajorType));
          CHECK_HR(outputMediaType_->SetGUID(MF_MT_SUBTYPE, mediaSubType));
          CHECK_HR(outputMediaType_->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, payloadType));
          CHECK_HR(outputMediaType_->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, avgBytePerSecond));
          CHECK_HR(outputMediaType_->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitDepth));
          CHECK_HR(outputMediaType_->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, samplesPerSec));
          CHECK_HR(outputMediaType_->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, nChannels));
          CHECK_HR(outputMediaType_->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, profile_Level));
          CHECK_HR(outputMediaType_->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, blockAlign));

          hr = (*transform)->SetOutputType(0, outputMediaType_, 0);
          if (SUCCEEDED(hr))
            break;
          else
            outputMediaType_->DeleteAllItems();
        }

        dwTypeIndex++;
      }

      return S_OK;
    }

    HRESULT MFAudioEncoder::addSinkWriter(const wchar_t* url) {
      /* Initialize sink writer that will write encoded aac output samples to the media sink (file_) */
      IMFByteStream* byteStream = NULL;
      CHECK_HR_STATUS(
        MFCreateFile(MF_ACCESSMODE_READWRITE, MF_OPENMODE_DELETE_IF_EXIST, MF_FILEFLAGS_NONE, url, &byteStream),
        FBCAPTURE_AUDIO_ENCODER_INIT_FAILED
      );
      CHECK_HR_STATUS(
        MFCreateADTSMediaSink(byteStream, outputMediaType_, &outputMediaSink_),
        FBCAPTURE_AUDIO_ENCODER_INIT_FAILED
      );
      CHECK_HR_STATUS(
        outputMediaSink_->GetStreamSinkByIndex(streamIndex_, &outputStreamSink_),
        FBCAPTURE_AUDIO_ENCODER_INIT_FAILED
      );
      CHECK_HR_STATUS(
        MFCreateSinkWriterFromMediaSink(outputMediaSink_, NULL, &outputSinkWriter_),
        FBCAPTURE_AUDIO_ENCODER_INIT_FAILED
      );
      CHECK_HR_STATUS(
        outputSinkWriter_->SetInputMediaType(streamIndex_, outputMediaType_, NULL),
        FBCAPTURE_AUDIO_ENCODER_INIT_FAILED
      );

      CHECK_HR_STATUS(outputSinkWriter_->BeginWriting(), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS MFAudioEncoder::initialize(WAVEFORMATEX *wavPWFX, const wchar_t* dstFile) {
      CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
      MFStartup(MF_VERSION);

      inWavFormat_ = wavPWFX;

      /* Initialize mf aac encoder transform object */

      CHECK_HR_STATUS(CoCreateInstance(CLSID_AACMFTEncoder, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&transform_)), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      CHECK_HR_STATUS(setTransformInputType(&transform_), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(setTransformOutputType(&transform_), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      IMFMediaType *tfOutMediaType;
      CHECK_HR_STATUS(transform_->GetOutputCurrentType(0, &tfOutMediaType), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      uint32_t waveFormatSize;
      CHECK_HR_STATUS(MFCreateWaveFormatExFromMFMediaType(tfOutMediaType, &outWavFormat_, &waveFormatSize),
                      FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      uint32_t mtUserDataSize;
      tfOutMediaType->GetBlobSize(MF_MT_USER_DATA, &mtUserDataSize);
      const auto mtUserData = static_cast<BYTE*>(malloc(mtUserDataSize));
      tfOutMediaType->GetBlob(MF_MT_USER_DATA, mtUserData, mtUserDataSize, NULL);
      outAacWavFormat_ = reinterpret_cast<HEAACWAVEFORMAT*>(mtUserData);

      CHECK_HR_STATUS(transform_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(transform_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      if (dstFile)
        CHECK_HR_STATUS(addSinkWriter(dstFile), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      outputBufferData_ = static_cast<BYTE*>(malloc(sizeof(float) * AudioBuffer::kMIX_BUFFER_LENGTH));
      outputBufferLength_ = 0;
      outputSamplePts_ = 0;
      outputSampleDuration_ = 0;

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS MFAudioEncoder::getSequenceParams(uint32_t* profileLevel, uint32_t* sampleRate, uint32_t* numChannels) const {
      if (outWavFormat_) {
        *profileLevel = MFAudioEncoder::kProfileLevel;
        *sampleRate = outWavFormat_->nSamplesPerSec;
        *numChannels = outWavFormat_->nChannels;
      } else
        return FBCAPTURE_AUDIO_ENCODER_GET_SEQUENCE_PARAMS_FAILED;

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS MFAudioEncoder::getEncodePacket(AudioEncodePacket** packet) {
      uint8_t* buffer;
      DWORD length;
      LONGLONG timestamp;
      LONGLONG duration;

      auto status = getOutputBuffer(&buffer, &length, &timestamp, &duration);
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed getting output encoded audio data", to_string(status));
        return status;
      }

      uint32_t profile_Level, sampleRate, numChannels;
      status = getSequenceParams(&profile_Level, &sampleRate, &numChannels);
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed getting sequence parameters for audio encoder", to_string(status));
        return status;
      }

      *packet = new AudioEncodePacket();
      (*packet)->buffer = static_cast<uint8_t*>(buffer);
      (*packet)->length = length;
      (*packet)->timestamp = timestamp;
      (*packet)->duration = duration;
      (*packet)->profileLevel = profile_Level;
      (*packet)->sampleRate = sampleRate;
      (*packet)->numChannels = numChannels;

      return status;
    }

    HRESULT MFAudioEncoder::processInput(const float* buffer, const uint32_t numSamples, LONGLONG pts, uint64_t duration) {
      // MF AAC encoder requires input sample to be of 16 bits per sample,
      // so we need to convert the 32 bit per sample input data to 16 bit per sample data
      // (https://msdn.microsoft.com/en-us/library/windows/desktop/dd742785(v=vs.85).aspx)
      const short* inputBufferData;
      AudioBuffer::convertTo16Bit(&inputBufferData, buffer, numSamples);
      DWORD bufferLength = numSamples * sizeof(short);

      BYTE* inputSampleData = static_cast<BYTE*>(malloc(bufferLength));

      if (!inputSample_) {
        CHECK_HR(MFCreateSample(&inputSample_));
      } else {
        inputSample_->RemoveAllBuffers();
      }

      CHECK_HR(MFCreateMemoryBuffer(bufferLength, &inputBuffer_));
      CHECK_HR(inputSample_->AddBuffer(inputBuffer_));

      CHECK_HR(inputBuffer_->Lock(&inputSampleData, NULL, NULL));

      memcpy(inputSampleData, inputBufferData, bufferLength);

      CHECK_HR(inputBuffer_->Unlock());
      CHECK_HR(inputBuffer_->SetCurrentLength(bufferLength));

      CHECK_HR(inputSample_->SetSampleTime(pts));
      CHECK_HR(inputSample_->SetSampleDuration(duration));

      const auto hr = transform_->ProcessInput(streamIndex_, inputSample_, NULL);

      SAFE_RELEASE(inputSample_);
      SAFE_RELEASE(inputBuffer_);

      return hr;
    }

    HRESULT MFAudioEncoder::processOutput() {
      DWORD outputStatus;
      MFT_OUTPUT_STREAM_INFO outputInfo = { 0 };
      MFT_OUTPUT_DATA_BUFFER output = { 0 };

      CHECK_HR(transform_->GetOutputStreamInfo(streamIndex_, &outputInfo));

      if (!outputSample_)
        CHECK_HR(MFCreateSample(&outputSample_));
      if (!outputBuffer_) {
        CHECK_HR(MFCreateMemoryBuffer(outputInfo.cbSize, &outputBuffer_));
        CHECK_HR(outputSample_->AddBuffer(outputBuffer_));
      }

      DWORD bufferMaxLength;
      CHECK_HR(outputBuffer_->GetMaxLength(&bufferMaxLength));
      if (bufferMaxLength < outputInfo.cbSize) {
        CHECK_HR(outputSample_->RemoveAllBuffers());
        CHECK_HR(MFCreateMemoryBuffer(outputInfo.cbSize, &outputBuffer_));
        CHECK_HR(outputSample_->AddBuffer(outputBuffer_));
      } else {
        outputBuffer_->SetCurrentLength(0);
      }

      output.pSample = outputSample_;

      return transform_->ProcessOutput(0, 1, &output, &outputStatus);
    }

    FBCAPTURE_STATUS MFAudioEncoder::encode(const float* buffer, const uint32_t numSamples, const LONGLONG pts, const uint64_t duration, EncStatus *encStatus) {
      auto status = FBCAPTURE_OK;
      auto hr = processInput(buffer, numSamples, pts, duration);

      if (hr == MF_E_NOTACCEPTING) {
        // This shouldn't happen since we drain right after processing input
        *encStatus = ENC_NOT_ACCEPTING;
        status = FBCAPTURE_AUDIO_ENCODER_UNEXPECTED_FAILURE;
        return status;
      } else if (FAILED(hr)) {
        *encStatus = ENC_FAILURE;
        status = FBCAPTURE_AUDIO_ENCODER_PROCESS_INPUT_FAILED;
        return status;
      } else
        *encStatus = ENC_SUCCESS;

      DWORD outputFlags;
      transform_->GetOutputStatus(&outputFlags);
      if (outputFlags != MFT_OUTPUT_STATUS_SAMPLE_READY) {
        *encStatus = ENC_NEED_MORE_INPUT;
        return status;
      }

      hr = processOutput();
      if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        *encStatus = ENC_NEED_MORE_INPUT;
        return status;
      } else if (FAILED(hr)) {
        *encStatus = ENC_FAILURE;
        status = FBCAPTURE_AUDIO_ENCODER_PROCESS_OUTPUT_FAILED;
        return status;
      } else
        *encStatus = ENC_SUCCESS;

      return status;
    }

    FBCAPTURE_STATUS MFAudioEncoder::getOutputBuffer(uint8_t** buffer, DWORD *length, LONGLONG* pts, LONGLONG* duration) {
      const auto status = FBCAPTURE_OK;

      CHECK_HR_STATUS(outputSample_->GetBufferByIndex(0, &outputBuffer_), FBCAPTURE_AUDIO_ENCODER_PROCESS_OUTPUT_FAILED);

      BYTE* encodedData;
      DWORD encodedDataLength;
      DWORD maxEncodedDataLength;
      CHECK_HR_STATUS(outputBuffer_->Lock(&encodedData, &maxEncodedDataLength, &encodedDataLength), FBCAPTURE_AUDIO_ENCODER_PROCESS_OUTPUT_FAILED);

      memcpy(outputBufferData_, encodedData, encodedDataLength);
      outputBufferLength_ = encodedDataLength;

      CHECK_HR_STATUS(outputBuffer_->Unlock(), FBCAPTURE_AUDIO_ENCODER_PROCESS_OUTPUT_FAILED);

      CHECK_HR_STATUS(outputSample_->GetSampleTime(&outputSamplePts_), FBCAPTURE_AUDIO_ENCODER_PROCESS_OUTPUT_FAILED);
      CHECK_HR_STATUS(outputSample_->GetSampleDuration(&outputSampleDuration_), FBCAPTURE_AUDIO_ENCODER_PROCESS_OUTPUT_FAILED);

      if (outputSinkWriter_)
        CHECK_HR_STATUS(outputSinkWriter_->WriteSample(streamIndex_, outputSample_), FBCAPTURE_AUDIO_ENCODER_PROCESS_OUTPUT_FAILED);

      *buffer = outputBufferData_;
      *length = outputBufferLength_;
      *pts = outputSamplePts_;
      *duration = outputSampleDuration_;

      SAFE_RELEASE(outputSample_);
      SAFE_RELEASE(outputBuffer_);

      return status;
    }

    FBCAPTURE_STATUS MFAudioEncoder::finalize() {
      if (outputByteStream_) {
        outputByteStream_->Close();
        SAFE_RELEASE(outputByteStream_);
      }
      SAFE_RELEASE(outputStreamSink_);
      SAFE_RELEASE(outputMediaSink_);

      SAFE_RELEASE(inputSample_);
      SAFE_RELEASE(inputBuffer_);
      SAFE_RELEASE(outputSample_);
      SAFE_RELEASE(outputBuffer_);

      if (outputSinkWriter_) {
        outputSinkWriter_->Finalize();
        SAFE_RELEASE(outputSinkWriter_);
      }

      outputBufferData_ = NULL;
      outputBufferLength_ = 0;
      outputSamplePts_ = 0;
      outputSampleDuration_ = 0;

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS MFAudioEncoder::encodeFile(const wstring srcFile, const wstring dstFile) {
      BOOL selected;
      DWORD streamCount = 0;
      auto objectType = MF_OBJECT_INVALID;

      CHECK_HR_STATUS(MFStartup(MF_VERSION), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      /* Create AAC Encoding media session */

      CHECK_HR_STATUS(MFCreateMediaSession(NULL, &mediaSession_), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(MFCreateSourceResolver(&sourceResolver_), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(sourceResolver_->CreateObjectFromURL((srcFile).c_str(), MF_RESOLUTION_MEDIASOURCE, NULL, &objectType, reinterpret_cast<IUnknown**>(&mediaSource_)), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      /* Create AAC Encoding topology object */

      CHECK_HR_STATUS(MFCreateTopology(&topology), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(mediaSource_->CreatePresentationDescriptor(&presDESC), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(presDESC->GetStreamDescriptorCount(&streamCount), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(presDESC->GetStreamDescriptorByIndex(0, &selected, &streamDESC), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      /* Configure encoding topology for Wav to AAC Encoding given input media wav format */

      CHECK_HR_STATUS(addSourceNode(topology, presDESC, streamDESC, &srcNode_), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(addTransformNode(topology, srcNode_, streamDESC, &transformNode_), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(addOutputNode(topology, transformNode_, &outputNode_, dstFile), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      CHECK_HR_STATUS(mediaSession_->SetTopology(0, topology), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      /* Handle media session event */

      IMFMediaEvent *mediaSessionEvent;
      const auto hr = handleMediaEvent(&mediaSessionEvent);
      SAFE_RELEASE(mediaSessionEvent);

      CHECK_HR_STATUS(hr, FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      shutdownSessions();

      return FBCAPTURE_OK;
    }

    HRESULT MFAudioEncoder::addSourceNode(IMFTopology* topology, IMFPresentationDescriptor* presDesc, IMFStreamDescriptor* streamDesc, IMFTopologyNode** node) const {
      CHECK_HR(MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &(*node)));
      CHECK_HR((*node)->SetUnknown(MF_TOPONODE_SOURCE, mediaSource_));
      CHECK_HR((*node)->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, presDesc));
      CHECK_HR((*node)->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, streamDesc));
      CHECK_HR(topology->AddNode(*node));
      return S_OK;
    }

    HRESULT MFAudioEncoder::addTransformNode(IMFTopology* topology, IMFTopologyNode* srcNode_, IMFStreamDescriptor* streamDesc, IMFTopologyNode** node) {
      IMFMediaTypeHandler* mediaTypeHandler;
      DWORD mediaTypeCount;
      uint32_t waveFormatSize;

      CHECK_HR(streamDesc->GetMediaTypeHandler(&mediaTypeHandler));
      CHECK_HR(mediaTypeHandler->GetMediaTypeCount(&mediaTypeCount));
      CHECK_HR(mediaTypeHandler->GetMediaTypeByIndex(streamIndex_, &inputMediaType_));
      CHECK_HR(MFCreateWaveFormatExFromMFMediaType(inputMediaType_, &inWavFormat_, &waveFormatSize));

      IMFActivate** mfActivate;
      uint32_t mftCount;
//      uint32_t mftEnumFlag = MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT | MFT_ENUM_FLAG_SORTANDFILTER | MFT_ENUM_FLAG_TRANSCODE_ONLY;

      CHECK_HR(MFTEnumEx(MFT_CATEGORY_AUDIO_ENCODER, 0, &MFAudioEncoder::kInInfo, &MFAudioEncoder::kOutInfo, &mfActivate, &mftCount));
      CHECK_HR(mfActivate[0]->ActivateObject(__uuidof(IMFTransform), (void**)&transform_));
      CHECK_HR(MFCreateTopologyNode(MF_TOPOLOGY_TRANSFORM_NODE, &(*node)));
      CHECK_HR((*node)->SetObject(transform_));
      CHECK_HR(topology->AddNode(*node));
      CHECK_HR(srcNode_->ConnectOutput(streamIndex_, *node, streamIndex_));

      CHECK_HR(MFCreateMediaType(&outputMediaType_));
      CHECK_HR(outputMediaType_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
      CHECK_HR(outputMediaType_->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC));
      CHECK_HR(outputMediaType_->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16));
      CHECK_HR(outputMediaType_->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100));
      CHECK_HR(outputMediaType_->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, inWavFormat_->nChannels));
      CHECK_HR(outputMediaType_->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 12000));
      CHECK_HR(outputMediaType_->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 1));
      CHECK_HR(outputMediaType_->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29));
      CHECK_HR(outputMediaType_->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1));
      CHECK_HR(outputMediaType_->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 0));
      CHECK_HR(outputMediaType_->SetUINT32(MF_MT_AVG_BITRATE, 96000));
      CHECK_HR(transform_->SetOutputType(streamIndex_, outputMediaType_, NULL));

      IMFMediaType* newMediaType;
      CHECK_HR(transform_->GetOutputCurrentType(streamIndex_, &newMediaType));
      CHECK_HR(MFCreateWaveFormatExFromMFMediaType(newMediaType, &outWavFormat_, &waveFormatSize));

      if (mfActivate != NULL) {
        for (uint32_t i = 0; i < mftCount; i++) {
          if (mfActivate[i] != NULL) {
            mfActivate[i]->Release();
          }
        }
      }

      CoTaskMemFree(mfActivate);
      CoTaskMemFree(inWavFormat_);
      CoTaskMemFree(outWavFormat_);

      return S_OK;
    }

    HRESULT MFAudioEncoder::addOutputNode(IMFTopology* topology, IMFTopologyNode* transformNode_, IMFTopologyNode** outputNode_, const wstring dstFile) {
      CHECK_HR(MFCreateFile(MF_ACCESSMODE_WRITE, MF_OPENMODE_DELETE_IF_EXIST, MF_FILEFLAGS_NONE, dstFile.c_str(), &outputByteStream_));
      CHECK_HR(MFCreateADTSMediaSink(outputByteStream_, outputMediaType_, &sink_));
      CHECK_HR(sink_->GetStreamSinkByIndex(streamIndex_, &outputStreamSink_));
      CHECK_HR(MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &(*outputNode_)));
      CHECK_HR((*outputNode_)->SetObject(outputStreamSink_));
      CHECK_HR(topology->AddNode(*outputNode_));
      CHECK_HR(transformNode_->ConnectOutput(streamIndex_, *outputNode_, streamIndex_));
      return S_OK;
    }

    HRESULT MFAudioEncoder::handleMediaEvent(IMFMediaEvent **mediaSessionEvent) const {
      PROPVARIANT var;
      HRESULT mediaSessionStatus;
      MediaEventType mediaEventType;
      auto topoStatus = static_cast<MF_TOPOSTATUS>(0);

      auto done = false;
      do {
        CHECK_HR(mediaSession_->GetEvent(0, mediaSessionEvent));
        CHECK_HR((*mediaSessionEvent)->GetStatus(&mediaSessionStatus));
        CHECK_HR((*mediaSessionEvent)->GetType(&mediaEventType));

        switch (mediaEventType) {
          case MESessionTopologyStatus:
            CHECK_HR((*mediaSessionEvent)->GetUINT32(MF_EVENT_TOPOLOGY_STATUS, (uint32_t*)&topoStatus));
            switch (topoStatus) {
              case MF_TOPOSTATUS_READY:
                // Fire up media playback (with no particular starting position)
                PropVariantInit(&var);
                var.vt = VT_EMPTY;
                CHECK_HR(mediaSession_->Start(&GUID_NULL, &var));
                PropVariantClear(&var);
                break;

              case MF_TOPOSTATUS_STARTED_SOURCE:
                break;

              case MF_TOPOSTATUS_ENDED:
                break;

              default:
                break;
            }
            break;

          case MESessionStarted:
            break;

          case MESessionEnded:
            CHECK_HR(mediaSession_->Stop());
            break;

          case MESessionStopped:
            CHECK_HR(mediaSession_->Close());
            break;

          case MESessionClosed:
            done = true;
            break;

          default:
            break;
        }
      } while (!done);

      return S_OK;
    }

    HRESULT MFAudioEncoder::shutdownSessions() {
      if (inWavFormat_) {
        CoTaskMemFree(inWavFormat_);
        inWavFormat_ = NULL;
      }
      if (outWavFormat_) {
        CoTaskMemFree(outWavFormat_);
        outWavFormat_ = NULL;
        outAacWavFormat_ = NULL;
      }

      if (mediaSession_) {
        mediaSession_->Shutdown();
        SAFE_RELEASE(mediaSession_);
      }
      if (mediaSource_) {
        mediaSource_->Shutdown();
        SAFE_RELEASE(mediaSource_);
      }
      if (sink_) {
        sink_->Shutdown();
        SAFE_RELEASE(sink_);
      }

      SAFE_RELEASE(sourceResolver_);
      SAFE_RELEASE(topology);
      SAFE_RELEASE(presDESC);
      SAFE_RELEASE(streamDESC);
      SAFE_RELEASE(srcNode_);
      SAFE_RELEASE(transformNode_);
      SAFE_RELEASE(outputNode_);

      SAFE_RELEASE(transform_);
      SAFE_RELEASE(inputMediaType_);
      SAFE_RELEASE(outputMediaType_);

      streamIndex_ = STREAM_INDEX;

      MFShutdown();
      CoUninitialize();

      return S_OK;
    }
  }
}
