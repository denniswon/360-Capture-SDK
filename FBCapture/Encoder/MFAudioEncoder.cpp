/****************************************************************************************************************

Filename	:	MFAudioEncoder.cpp
Content		:
Copyright	:

****************************************************************************************************************/

#include "MFAudioEncoder.h"
#include "Common.h"
#include "Log.h"

namespace FBCapture {
  namespace Audio {

    const uint32_t MFAudioEncoder::profileLevel = 0x2;

    MFAudioEncoder::MFAudioEncoder() :
      inWavFormat(NULL),
      outWavFormat(NULL),
      outAacWavFormat(NULL),
      mediaSession(NULL),
      mediaSource(NULL),
      sink(NULL),
      sourceResolver(NULL),
      topology(NULL),
      presDESC(NULL),
      streamDESC(NULL),
      srcNode(NULL),
      transformNode(NULL),
      outputNode(NULL),
      transform(NULL),
      inputMediaType(NULL),
      outputMediaType(NULL),
      streamIndex(STREAM_INDEX),
      outputByteStream(NULL),
      outputStreamSink(NULL),
      outputMediaSink(NULL),
      inputSample(NULL),
      inputBuffer(NULL),
      outputSample(NULL),
      outputBuffer(NULL),
      outputSinkWriter(NULL),
      outputBufferData(NULL),
      outputBufferLength(0),
      outputSamplePts(0),
      outputSampleDuration(0) {}

    MFAudioEncoder::~MFAudioEncoder() {
      finalize();
      shutdownSessions();
    }

    HRESULT MFAudioEncoder::setTransformInputType(IMFTransform** transform) {
      HRESULT hr = S_OK;
      DWORD dwTypeIndex = 0;
      IMFMediaType *ppType;

      GUID mediaMajorType;
      GUID mediaSubType;
      uint32_t bitDepth;
      uint32_t samplesPerSec;
      uint32_t nChannels;

      CHECK_HR(MFCreateMediaType(&inputMediaType));

      while (true) {
        hr = (*transform)->GetInputAvailableType(0, dwTypeIndex, &ppType);
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
            samplesPerSec == inWavFormat->nSamplesPerSec &&
            nChannels == AudioBuffer::kSTEREO) {

          CHECK_HR(inputMediaType->SetGUID(MF_MT_MAJOR_TYPE, mediaMajorType));
          CHECK_HR(inputMediaType->SetGUID(MF_MT_SUBTYPE, mediaSubType));
          CHECK_HR(inputMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitDepth));
          CHECK_HR(inputMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, samplesPerSec));
          CHECK_HR(inputMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, nChannels));

          hr = (*transform)->SetInputType(streamIndex, inputMediaType, NULL);
          if (SUCCEEDED(hr))
            break;
          else
            inputMediaType->DeleteAllItems();
        }

        dwTypeIndex++;
      }

      return hr;
    }

    HRESULT MFAudioEncoder::setTransformOutputType(IMFTransform** transform) {
      HRESULT hr = S_OK;
      DWORD dwTypeIndex = 0;
      IMFMediaType *ppType;

      GUID mediaMajorType;
      GUID mediaSubType;
      uint32_t payloadType;
      uint32_t nChannels;
      uint32_t avgBytePerSecond;
      uint32_t bitDepth;
      uint32_t samplesPerSec;
      uint32_t profileLevel;
      uint32_t blockAlign;

      uint32_t inputBitDepth;
      uint32_t inputSampleRate;
      uint32_t inputNumChannels;

      /* Output type requirement for MF AAC Encoder https://msdn.microsoft.com/en-us/library/windows/desktop/dd742785(v=vs.85).aspx */
      inputMediaType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &inputBitDepth);
      inputMediaType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &inputSampleRate);
      inputMediaType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &inputNumChannels);

      CHECK_HR(MFCreateMediaType(&outputMediaType));

      while (true) {
        hr = (*transform)->GetOutputAvailableType(0, dwTypeIndex, &ppType);
        if (hr == MF_E_NO_MORE_TYPES) break;

        ppType->GetMajorType(&mediaMajorType);
        ppType->GetGUID(MF_MT_SUBTYPE, &mediaSubType);
        ppType->GetUINT32(MF_MT_AAC_PAYLOAD_TYPE, &payloadType);
        ppType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &nChannels);
        ppType->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &avgBytePerSecond);
        ppType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitDepth);
        ppType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &samplesPerSec);
        ppType->GetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, &profileLevel);
        ppType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlign);

        if (mediaMajorType == MFMediaType_Audio &&
            mediaSubType == MFAudioFormat_AAC &&
            bitDepth == inputBitDepth &&
            samplesPerSec == inputSampleRate &&
            nChannels == inputNumChannels &&
            payloadType == AAC_PAYLOAD_TYPE) {

          CHECK_HR(outputMediaType->SetGUID(MF_MT_MAJOR_TYPE, mediaMajorType));
          CHECK_HR(outputMediaType->SetGUID(MF_MT_SUBTYPE, mediaSubType));
          CHECK_HR(outputMediaType->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, payloadType));
          CHECK_HR(outputMediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, avgBytePerSecond));
          CHECK_HR(outputMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bitDepth));
          CHECK_HR(outputMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, samplesPerSec));
          CHECK_HR(outputMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, nChannels));
          CHECK_HR(outputMediaType->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, profileLevel));
          CHECK_HR(outputMediaType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, blockAlign));

          hr = (*transform)->SetOutputType(0, outputMediaType, 0);
          if (SUCCEEDED(hr))
            break;
          else
            outputMediaType->DeleteAllItems();
        }

        dwTypeIndex++;
      }

      return hr;
    }

    HRESULT MFAudioEncoder::addSinkWriter(const wchar_t* url) {
      HRESULT hr = S_OK;
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      /* Initialize sink writer that will write encoded aac output samples to the media sink (file) */
      IMFByteStream* byteStream = NULL;
      CHECK_HR_STATUS(
        MFCreateFile(MF_ACCESSMODE_READWRITE, MF_OPENMODE_DELETE_IF_EXIST, MF_FILEFLAGS_NONE, url, &byteStream),
        FBCAPTURE_AUDIO_ENCODER_INIT_FAILED
      );
      CHECK_HR_STATUS(
        MFCreateADTSMediaSink(byteStream, outputMediaType, &outputMediaSink),
        FBCAPTURE_AUDIO_ENCODER_INIT_FAILED
      );
      CHECK_HR_STATUS(
        outputMediaSink->GetStreamSinkByIndex(streamIndex, &outputStreamSink),
        FBCAPTURE_AUDIO_ENCODER_INIT_FAILED
      );
      CHECK_HR_STATUS(
        MFCreateSinkWriterFromMediaSink(outputMediaSink, NULL, &outputSinkWriter),
        FBCAPTURE_AUDIO_ENCODER_INIT_FAILED
      );
      CHECK_HR_STATUS(
        outputSinkWriter->SetInputMediaType(streamIndex, outputMediaType, NULL),
        FBCAPTURE_AUDIO_ENCODER_INIT_FAILED
      );

      CHECK_HR_STATUS(outputSinkWriter->BeginWriting(), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      return status;
    }

    FBCAPTURE_STATUS MFAudioEncoder::initialize(WAVEFORMATEX *wavPWFX, const wchar_t* dstFile) {
      CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
      MFStartup(MF_VERSION);

      HRESULT hr = S_OK;
      FBCAPTURE_STATUS status = FBCAPTURE_OK;
      inWavFormat = wavPWFX;

      /* Initialize mf aac encoder transform object */

      CHECK_HR_STATUS(CoCreateInstance(CLSID_AACMFTEncoder, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&transform)), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      CHECK_HR_STATUS(setTransformInputType(&transform), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(setTransformOutputType(&transform), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      IMFMediaType *tfOutMediaType;
      CHECK_HR_STATUS(transform->GetOutputCurrentType(0, &tfOutMediaType), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      uint32_t waveFormatSize;
      CHECK_HR_STATUS(MFCreateWaveFormatExFromMFMediaType(tfOutMediaType, &outWavFormat, &waveFormatSize), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      uint32_t mtUserDataSize;
      tfOutMediaType->GetBlobSize(MF_MT_USER_DATA, &mtUserDataSize);
      BYTE* mtUserData = (BYTE*)malloc(mtUserDataSize);
      tfOutMediaType->GetBlob(MF_MT_USER_DATA, mtUserData, mtUserDataSize, NULL);
      outAacWavFormat = (HEAACWAVEFORMAT*)mtUserData;

      CHECK_HR_STATUS(transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      if (dstFile)
        CHECK_HR_STATUS(addSinkWriter(dstFile), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      outputBufferData = (BYTE*)malloc(sizeof(float) * AudioBuffer::kMIX_BUFFER_LENGTH);
      outputBufferLength = 0;
      outputSamplePts = 0;
      outputSampleDuration = 0;

      return status;
    }

    FBCAPTURE_STATUS MFAudioEncoder::getSequenceParams(uint32_t* profileLevel, uint32_t* sampleRate, uint32_t* numChannels) {
      if (outWavFormat) {
        *profileLevel = MFAudioEncoder::profileLevel;
        *sampleRate = outWavFormat->nSamplesPerSec;
        *numChannels = outWavFormat->nChannels;
      } else
        return FBCAPTURE_AUDIO_ENCODER_GET_SEQUENCE_PARAMS_FAILED;

      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS MFAudioEncoder::getEncodePacket(AudioEncodePacket** packet) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      uint8_t* buffer;
      DWORD length;
      LONGLONG timestamp;
      LONGLONG duration;

      status = getOutputBuffer(&buffer, &length, &timestamp, &duration);
      // DEBUG_LOG_VAR("  Audio packet pts ", to_string((uint32_t)(audioPacketPts / pow(10, 4))) + " ms");
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed getting output encoded audio data", to_string(status));
        return status;
      }

      uint32_t profileLevel, sampleRate, numChannels;
      status = getSequenceParams(&profileLevel, &sampleRate, &numChannels);
      if (status != FBCAPTURE_OK) {
        DEBUG_ERROR_VAR("Failed getting sequence parameters for audio encoder", to_string(status));
        return status;
      }

      *packet = new AudioEncodePacket();
      (*packet)->buffer = static_cast<uint8_t*>(buffer);
      (*packet)->length = length;
      (*packet)->timestamp = timestamp;
      (*packet)->duration = duration;
      (*packet)->profileLevel = profileLevel;
      (*packet)->sampleRate = sampleRate;
      (*packet)->numChannels = numChannels;

      return status;
    }

    HRESULT MFAudioEncoder::processInput(const float* buffer, uint32_t numSamples, LONGLONG pts, uint64_t duration) {
      HRESULT hr = S_OK;

      // MF AAC encoder requires input sample to be of 16 bits per sample,
      // so we need to convert the 32 bit per sample input data to 16 bit per sample data
      // (https://msdn.microsoft.com/en-us/library/windows/desktop/dd742785(v=vs.85).aspx)
      const short* inputBufferData;
      AudioBuffer::convertTo16Bit(&inputBufferData, buffer, numSamples);
      DWORD bufferLength = numSamples * sizeof(short);

      BYTE* inputSampleData = (BYTE*)malloc(bufferLength);

      if (!inputSample) {
        CHECK_HR(MFCreateSample(&inputSample));
      } else {
        inputSample->RemoveAllBuffers();
      }

      CHECK_HR(MFCreateMemoryBuffer(bufferLength, &inputBuffer));
      CHECK_HR(inputSample->AddBuffer(inputBuffer));

      CHECK_HR(inputBuffer->Lock(&inputSampleData, NULL, NULL));

      memcpy(inputSampleData, inputBufferData, bufferLength);

      CHECK_HR(inputBuffer->Unlock());
      CHECK_HR(inputBuffer->SetCurrentLength(bufferLength));

      CHECK_HR(inputSample->SetSampleTime(pts));
      CHECK_HR(inputSample->SetSampleDuration(duration));

      hr = transform->ProcessInput(streamIndex, inputSample, NULL);

      SAFE_RELEASE(inputSample);
      SAFE_RELEASE(inputBuffer);

      return hr;
    }

    HRESULT MFAudioEncoder::processOutput() {
      HRESULT hr = S_OK;

      DWORD outputStatus;
      MFT_OUTPUT_STREAM_INFO outputInfo = { 0 };
      MFT_OUTPUT_DATA_BUFFER output = { 0 };

      CHECK_HR(transform->GetOutputStreamInfo(streamIndex, &outputInfo));

      if (!outputSample)
        CHECK_HR(MFCreateSample(&outputSample));
      if (!outputBuffer) {
        CHECK_HR(MFCreateMemoryBuffer(outputInfo.cbSize, &outputBuffer));
        CHECK_HR(outputSample->AddBuffer(outputBuffer));
      }

      DWORD bufferMaxLength;
      CHECK_HR(outputBuffer->GetMaxLength(&bufferMaxLength));
      if (bufferMaxLength < outputInfo.cbSize) {
        CHECK_HR(outputSample->RemoveAllBuffers());
        CHECK_HR(MFCreateMemoryBuffer(outputInfo.cbSize, &outputBuffer));
        CHECK_HR(outputSample->AddBuffer(outputBuffer));
      } else {
        outputBuffer->SetCurrentLength(0);
      }

      output.pSample = outputSample;

      return transform->ProcessOutput(0, 1, &output, &outputStatus);
    }

    FBCAPTURE_STATUS MFAudioEncoder::encode(const float* buffer, uint32_t numSamples, LONGLONG pts, uint64_t duration, EncStatus *encStatus) {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      HRESULT hr = processInput(buffer, numSamples, pts, duration);

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
      hr = transform->GetOutputStatus(&outputFlags);
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
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      CHECK_HR_STATUS(outputSample->GetBufferByIndex(0, &outputBuffer), FBCAPTURE_AUDIO_ENCODER_PROCESS_OUTPUT_FAILED);

      BYTE* encodedData;
      DWORD encodedDataLength;
      DWORD maxEncodedDataLength;
      CHECK_HR_STATUS(outputBuffer->Lock(&encodedData, &maxEncodedDataLength, &encodedDataLength), FBCAPTURE_AUDIO_ENCODER_PROCESS_OUTPUT_FAILED);

      memcpy(outputBufferData, encodedData, encodedDataLength);
      outputBufferLength = encodedDataLength;

      CHECK_HR_STATUS(outputBuffer->Unlock(), FBCAPTURE_AUDIO_ENCODER_PROCESS_OUTPUT_FAILED);

      CHECK_HR_STATUS(outputSample->GetSampleTime(&outputSamplePts), FBCAPTURE_AUDIO_ENCODER_PROCESS_OUTPUT_FAILED);
      CHECK_HR_STATUS(outputSample->GetSampleDuration(&outputSampleDuration), FBCAPTURE_AUDIO_ENCODER_PROCESS_OUTPUT_FAILED);

      if (outputSinkWriter)
        CHECK_HR_STATUS(outputSinkWriter->WriteSample(streamIndex, outputSample), FBCAPTURE_AUDIO_ENCODER_PROCESS_OUTPUT_FAILED);

      *buffer = outputBufferData;
      *length = outputBufferLength;
      *pts = outputSamplePts;
      *duration = outputSampleDuration;

      SAFE_RELEASE(outputSample);
      SAFE_RELEASE(outputBuffer);

      return status;
    }

    FBCAPTURE_STATUS MFAudioEncoder::finalize() {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      if (outputByteStream) {
        outputByteStream->Close();
        SAFE_RELEASE(outputByteStream);
      }
      SAFE_RELEASE(outputStreamSink);
      SAFE_RELEASE(outputMediaSink);

      SAFE_RELEASE(inputSample);
      SAFE_RELEASE(inputBuffer);
      SAFE_RELEASE(outputSample);
      SAFE_RELEASE(outputBuffer);

      if (outputSinkWriter) {
        outputSinkWriter->Finalize();
        SAFE_RELEASE(outputSinkWriter);
      }

      outputBufferData = NULL;
      outputBufferLength = 0;
      outputSamplePts = 0;
      outputSampleDuration = 0;

      return status;
    }

    FBCAPTURE_STATUS MFAudioEncoder::encodeFile(const wstring srcFile, const wstring dstFile) {
      HRESULT hr = S_OK;
      FBCAPTURE_STATUS status = FBCAPTURE_OK;
      BOOL selected;
      DWORD streamCount = 0;
      MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;

      CHECK_HR_STATUS(MFStartup(MF_VERSION), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      /* Create AAC Encoding media session */

      CHECK_HR_STATUS(MFCreateMediaSession(NULL, &mediaSession), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(MFCreateSourceResolver(&sourceResolver), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(sourceResolver->CreateObjectFromURL((srcFile).c_str(), MF_RESOLUTION_MEDIASOURCE, NULL, &ObjectType, (IUnknown**)&mediaSource), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      /* Create AAC Encoding topology object */

      CHECK_HR_STATUS(MFCreateTopology(&topology), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(mediaSource->CreatePresentationDescriptor(&presDESC), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(presDESC->GetStreamDescriptorCount(&streamCount), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(presDESC->GetStreamDescriptorByIndex(0, &selected, &streamDESC), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      /* Configure encoding topology for Wav to AAC Encoding given input media wav format */

      CHECK_HR_STATUS(addSourceNode(topology, presDESC, streamDESC, &srcNode), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(addTransformNode(topology, srcNode, streamDESC, &transformNode), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);
      CHECK_HR_STATUS(addOutputNode(topology, transformNode, &outputNode, dstFile), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      CHECK_HR_STATUS(mediaSession->SetTopology(0, topology), FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      /* Handle media session event */

      IMFMediaEvent *mediaSessionEvent;
      hr = handleMediaEvent(&mediaSessionEvent);
      SAFE_RELEASE(mediaSessionEvent);

      CHECK_HR_STATUS(hr, FBCAPTURE_AUDIO_ENCODER_INIT_FAILED);

      shutdownSessions();

      return status;
    }

    HRESULT MFAudioEncoder::addSourceNode(IMFTopology* topology, IMFPresentationDescriptor* presDesc, IMFStreamDescriptor* streamDesc, IMFTopologyNode** node) {
      HRESULT hr = S_OK;

      CHECK_HR(MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &(*node)));
      CHECK_HR((*node)->SetUnknown(MF_TOPONODE_SOURCE, mediaSource));
      CHECK_HR((*node)->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, presDesc));
      CHECK_HR((*node)->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, streamDesc));
      CHECK_HR(topology->AddNode(*node));

      return hr;
    }

    HRESULT MFAudioEncoder::addTransformNode(IMFTopology* topology, IMFTopologyNode* srcNode, IMFStreamDescriptor* streamDesc, IMFTopologyNode** node) {
      HRESULT hr = S_OK;

      IMFMediaTypeHandler* mediaTypeHandler;
      DWORD mediaTypeCount;
      uint32_t waveFormatSize;

      CHECK_HR(streamDesc->GetMediaTypeHandler(&mediaTypeHandler));
      CHECK_HR(mediaTypeHandler->GetMediaTypeCount(&mediaTypeCount));
      CHECK_HR(mediaTypeHandler->GetMediaTypeByIndex(streamIndex, &inputMediaType));
      CHECK_HR(MFCreateWaveFormatExFromMFMediaType(inputMediaType, &inWavFormat, &waveFormatSize));

      IMFActivate** mfActivate;
      uint32_t mftCount;
      uint32_t mftEnumFlag = MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT | MFT_ENUM_FLAG_SORTANDFILTER | MFT_ENUM_FLAG_TRANSCODE_ONLY;

      CHECK_HR(MFTEnumEx(MFT_CATEGORY_AUDIO_ENCODER, 0, &inInfo, &outInfo, &mfActivate, &mftCount));
      CHECK_HR(mfActivate[0]->ActivateObject(__uuidof(IMFTransform), (void**)&transform));
      CHECK_HR(MFCreateTopologyNode(MF_TOPOLOGY_TRANSFORM_NODE, &(*node)));
      CHECK_HR((*node)->SetObject(transform));
      CHECK_HR(topology->AddNode(*node));
      CHECK_HR(srcNode->ConnectOutput(streamIndex, *node, streamIndex));

      CHECK_HR(MFCreateMediaType(&outputMediaType));
      CHECK_HR(outputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
      CHECK_HR(outputMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC));
      CHECK_HR(outputMediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16));
      CHECK_HR(outputMediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100));
      CHECK_HR(outputMediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, inWavFormat->nChannels));
      CHECK_HR(outputMediaType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 12000));
      CHECK_HR(outputMediaType->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 1));
      CHECK_HR(outputMediaType->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29));
      CHECK_HR(outputMediaType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1));
      CHECK_HR(outputMediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 0));
      CHECK_HR(outputMediaType->SetUINT32(MF_MT_AVG_BITRATE, 96000));
      CHECK_HR(transform->SetOutputType(streamIndex, outputMediaType, NULL));

      IMFMediaType* newMediaType;
      CHECK_HR(transform->GetOutputCurrentType(streamIndex, &newMediaType));
      CHECK_HR(MFCreateWaveFormatExFromMFMediaType(newMediaType, &outWavFormat, &waveFormatSize));

      if (mfActivate != NULL) {
        for (uint32_t i = 0; i < mftCount; i++) {
          if (mfActivate[i] != NULL) {
            mfActivate[i]->Release();
          }
        }
      }

      CoTaskMemFree(mfActivate);
      CoTaskMemFree(inWavFormat);
      CoTaskMemFree(outWavFormat);

      return hr;
    }

    HRESULT MFAudioEncoder::addOutputNode(IMFTopology* topology, IMFTopologyNode* transformNode, IMFTopologyNode** outputNode, const wstring dstFile) {
      HRESULT hr = S_OK;

      CHECK_HR(MFCreateFile(MF_ACCESSMODE_WRITE, MF_OPENMODE_DELETE_IF_EXIST, MF_FILEFLAGS_NONE, dstFile.c_str(), &outputByteStream));
      CHECK_HR(MFCreateADTSMediaSink(outputByteStream, outputMediaType, &sink));
      CHECK_HR(sink->GetStreamSinkByIndex(streamIndex, &outputStreamSink));
      CHECK_HR(MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &(*outputNode)));
      CHECK_HR((*outputNode)->SetObject(outputStreamSink));
      CHECK_HR(topology->AddNode(*outputNode));
      CHECK_HR(transformNode->ConnectOutput(streamIndex, *outputNode, streamIndex));

      return hr;
    }

    HRESULT MFAudioEncoder::handleMediaEvent(IMFMediaEvent **mediaSessionEvent) {
      HRESULT hr = S_OK;

      PROPVARIANT var;
      HRESULT mediaSessionStatus;
      MediaEventType mediaEventType;
      MF_TOPOSTATUS topo_status = (MF_TOPOSTATUS)0;

      bool done = false;
      do {
        CHECK_HR(mediaSession->GetEvent(0, mediaSessionEvent));
        CHECK_HR((*mediaSessionEvent)->GetStatus(&mediaSessionStatus));
        CHECK_HR((*mediaSessionEvent)->GetType(&mediaEventType));

        switch (mediaEventType) {
          case MESessionTopologyStatus:
            CHECK_HR((*mediaSessionEvent)->GetUINT32(MF_EVENT_TOPOLOGY_STATUS, (uint32_t*)&topo_status));
            switch (topo_status) {
              case MF_TOPOSTATUS_READY:
                // Fire up media playback (with no particular starting position)
                PropVariantInit(&var);
                var.vt = VT_EMPTY;
                CHECK_HR(mediaSession->Start(&GUID_NULL, &var));
                PropVariantClear(&var);
                break;

              case MF_TOPOSTATUS_STARTED_SOURCE:
                break;

              case MF_TOPOSTATUS_ENDED:
                break;
            }
            break;

          case MESessionStarted:
            break;

          case MESessionEnded:
            CHECK_HR(mediaSession->Stop());
            break;

          case MESessionStopped:
            CHECK_HR(mediaSession->Close());
            break;

          case MESessionClosed:
            done = TRUE;
            break;

          default:
            break;
        }
      } while (!done);

      return hr;
    }

    HRESULT MFAudioEncoder::shutdownSessions() {
      if (inWavFormat) {
        CoTaskMemFree(inWavFormat);
        inWavFormat = NULL;
      }
      if (outWavFormat) {
        CoTaskMemFree(outWavFormat);
        outWavFormat = NULL;
        outAacWavFormat = NULL;
      }

      if (mediaSession) {
        mediaSession->Shutdown();
        SAFE_RELEASE(mediaSession);
      }
      if (mediaSource) {
        mediaSource->Shutdown();
        SAFE_RELEASE(mediaSource);
      }
      if (sink) {
        sink->Shutdown();
        SAFE_RELEASE(sink);
      }

      SAFE_RELEASE(sourceResolver);
      SAFE_RELEASE(topology);
      SAFE_RELEASE(presDESC);
      SAFE_RELEASE(streamDESC);
      SAFE_RELEASE(srcNode);
      SAFE_RELEASE(transformNode);
      SAFE_RELEASE(outputNode);

      SAFE_RELEASE(transform);
      SAFE_RELEASE(inputMediaType);
      SAFE_RELEASE(outputMediaType);

      streamIndex = STREAM_INDEX;

      MFShutdown();
      CoUninitialize();

      return S_OK;
    }
  }
}
