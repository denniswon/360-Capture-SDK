#pragma once

#define FBCAPTURE_SESSION_STATUS			      0
#define FBCAPTURE_GENERIC_ERROR			        100
#define FBCAPTURE_GPU_ENCODER_ERROR			    200
#define FBCAPTURE_AUDIO_CAPTURE_ERROR			  300
#define FBCAPTURE_AUDIO_ENCODER_ERROR			  400
#define FBCAPTURE_MUXING_ERROR           		500
#define FBCAPTURE_FLV_PACKETIZER_ERROR		  600
#define FBCAPTURE_RTMP_ERROR						    700
#define FBCAPTURE_METADATA_ERROR            800

typedef enum {
  // FBCapture session status
  FBCAPTURE_OK = FBCAPTURE_SESSION_STATUS,
  FBCAPTURE_SESSION_INITIALIZED,
  FBCAPTURE_SESSION_ACTIVE,
  FBCAPTURE_SESSION_FAIL,

  // Common error codes
  FBCAPTURE_OUTPUT_FILE_OPEN_FAILED = FBCAPTURE_GENERIC_ERROR,
  FBCAPTURE_THREAD_NOT_INITIALIZED,
  FBCAPTURE_UNKNOWN_ENCODE_PACKET_TYPE,
  FBCAPTURE_ENCODER_NEED_MORE_INPUT,
  FBCAPTURE_INVALID_FUNCTION_CALL,
  FBCAPTURE_INVALID_SESSION_STATUS,
  FBCAPTURE_UNEXPECTED_EXCEPTION,

  // Video/Image encoding specific error codes
  FBCAPTURE_GPU_ENCODER_UNSUPPORTED_DRIVER = FBCAPTURE_GPU_ENCODER_ERROR,
  FBCAPTURE_GPU_ENCODER_INIT_FAILED,
  FBCAPTURE_GPU_ENCODER_NULL_TEXTURE_POINTER,
  FBCAPTURE_GPU_ENCODER_MAP_INPUT_TEXTURE_FAILED,
  FBCAPTURE_GPU_ENCODER_ENCODE_FRAME_FAILED,
  FBCAPTURE_GPU_ENCODER_BUFFER_FULL,
  FBCAPTURE_GPU_ENCODER_BUFFER_EMPTY,
  FBCAPTURE_GPU_ENCODER_PROCESS_OUTPUT_FAILED,
  FBCAPTURE_GPU_ENCODER_GET_SEQUENCE_PARAMS_FAILED,
  FBCAPTURE_GPU_ENCODER_FINALIZE_FAILED,

  // WIC specific error codes
  FBCAPTURE_GPU_ENCODER_WIC_SAVE_IMAGE_FAILED,

  // Audio capture specific error codes
  FBCAPTURE_AUDIO_CAPTURE_INIT_FAILED = FBCAPTURE_AUDIO_CAPTURE_ERROR,
  FBCAPTURE_AUDIO_CAPTURE_NOT_INITIALIZED,
  FBCAPTURE_AUDIO_CAPTURE_UNSUPPORTED_AUDIO_SOURCE,
  FBCAPTURE_AUDIO_CAPTURE_PACKETS_FAILED,
  FBCAPTURE_AUDIO_CAPTURE_STOP_FAILED,

  // Audio encoding specific error codes
  FBCAPTURE_AUDIO_ENCODER_INIT_FAILED = FBCAPTURE_AUDIO_ENCODER_ERROR,
  FBCAPTURE_AUDIO_ENCODER_PROCESS_INPUT_FAILED,
  FBCAPTURE_AUDIO_ENCODER_PROCESS_OUTPUT_FAILED,
  FBCAPTURE_AUDIO_ENCODER_UNEXPECTED_FAILURE,
  FBCAPTURE_AUDIO_ENCODER_GET_SEQUENCE_PARAMS_FAILED,
  FBCAPTURE_AUDIO_ENCODER_FINALIZE_FAILED,

  // WAMEDIA muxing specific error codes
  FBCAPTURE_WAMDEIA_MUXING_FAILED = FBCAPTURE_MUXING_ERROR,

  // FLV Packetizer specific error codes
  FBCAPTURE_FLV_SET_HEADER_FAILED = FBCAPTURE_FLV_PACKETIZER_ERROR,
  FBCAPTURE_FLV_SET_AAC_SEQ_HEADER_FAILED,
  FBCAPTURE_FLV_SET_AAC_DATA_FAILED,
  FBCAPTURE_FLV_SET_AVC_SEQ_HEADER_FAILED,
  FBCAPTURE_FLV_SET_AVC_DATA_FAILED,

  // RTMP specific error codes
  FBCAPTURE_RTMP_INVALID_FLV_FILE = FBCAPTURE_RTMP_ERROR,
  FBCAPTURE_RTMP_INVALID_FLV_HEADER,
  FBCAPTURE_RTMP_INVALID_STREAM_URL,
  FBCAPTURE_RTMP_CONNECTION_FAILED,
  FBCAPTURE_RTMP_DISCONNECTED,
  FBCAPTURE_RTMP_SEND_PACKET_FAILED,

  // SpatialMedia Metadata error codes
  FBCAPTURE_METADATA_INVALID = FBCAPTURE_METADATA_ERROR,
  FBCAPTURE_METADATA_INJECTION_NOT_READY,
  FBCAPTURE_METADATA_INJECTION_FAIL,

} FBCAPTURE_STATUS;
