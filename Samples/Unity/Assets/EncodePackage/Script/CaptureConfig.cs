using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace FBCapture {

    using DestinationURL = String;

    // For screenshot, FBCaptureConfig options do not apply as they are only applicable to video encoding/streaming.
    // If only interested in screenshot not video in the application, simply pass in an empty FBCaptureConfig object.
    struct FBCaptureConfig {

        // Encoding option [required]
        public int bitrate;
        public int fps;
        public int gop;

        // Capture texture option [optional]
        public bool flipTexture;

        // Audio capture option [optional]
        public bool mute;
        public bool mixMic;
        public bool useRiftAudioSources;

        // Enable async encoding mode for non-blocking FBCaptureEncodeFrame() call [optional]
        public bool enableAsyncMode;

        public FBCaptureConfig(
            int bitrate,
            int fps,
            int gop,
            bool flipTexture = false,
            bool mute = false,
            bool mixMic = false,
            bool useRiftAudioSources = false,
            bool enableAsyncMode = false
        ) {
            this.bitrate = bitrate;
            this.fps = fps;
            this.gop = gop;
            this.flipTexture = flipTexture;
            this.mute = mute;
            this.mixMic = mixMic;
            this.useRiftAudioSources = useRiftAudioSources;
            this.enableAsyncMode = enableAsyncMode;
        }
    }

    public enum ErrorType {
        INITIALIZE_FAILED,
        START_SESSION_FAILED,
        STOP_SESSION_FAIL,
        ENCODE_FRAME_FAILED,
        SCREENSHOT_FAILED,
        RELEASE_FAILED,
    }

    public enum FBCAPTURE_STATUS {
        // FBCapture session status
        OK = 0,
        SESSION_INITIALIZED,
        SESSION_ACTIVE,
        SESSION_FAIL,

        // Common error codes
        OUTPUT_FILE_OPEN_FAILED = 100,
        THREAD_NOT_INITIALIZED,
        UNKNOWN_ENCODE_PACKET_TYPE,
        ENCODER_NEED_MORE_INPUT,
        INVALID_FUNCTION_CALL,
        INVALID_SESSION_STATUS,
        UNEXPECTED_EXCEPTION,

        // Video/Image encoding specific error codes
        GPU_ENCODER_UNSUPPORTED_DRIVER = 200,
        GPU_ENCODER_INIT_FAILED,
        GPU_ENCODER_NULL_TEXTURE_POINTER,
        GPU_ENCODER_MAP_INPUT_TEXTURE_FAILED,
        GPU_ENCODER_ENCODE_FRAME_FAILED,
        GPU_ENCODER_BUFFER_FULL,
        GPU_ENCODER_BUFFER_EMPTY,
        GPU_ENCODER_PROCESS_OUTPUT_FAILED,
        GPU_ENCODER_GET_SEQUENCE_PARAMS_FAILED,
        GPU_ENCODER_FINALIZE_FAILED,

        // WIC specific error codes
        GPU_ENCODER_WIC_SAVE_IMAGE_FAILED,

        // Audio capture specific error codes
        AUDIO_CAPTURE_INIT_FAILED = 300,
        AUDIO_CAPTURE_NOT_INITIALIZED,
        AUDIO_CAPTURE_UNSUPPORTED_AUDIO_SOURCE,
        AUDIO_CAPTURE_PACKETS_FAILED,
        AUDIO_CAPTURE_STOP_FAILED,

        // Audio encoding specific error codes
        AUDIO_ENCODER_INIT_FAILED = 400,
        AUDIO_ENCODER_PROCESS_INPUT_FAILED,
        AUDIO_ENCODER_PROCESS_OUTPUT_FAILED,
        AUDIO_ENCODER_GET_SEQUENCE_PARAMS_FAILED,
        AUDIO_ENCODER_FINALIZE_FAILED,

        // WAMEDIA muxing specific error codes
        WAMDEIA_MUXING_FAILED = 500,

        // FLV Packetizer specific error codes
        FLV_SET_HEADER_FAILED = 600,
        FLV_SET_AAC_SEQ_HEADER_FAILED,
        FLV_SET_AAC_DATA_FAILED,
        FLV_SET_AVC_SEQ_HEADER_FAILED,
        FLV_SET_AVC_DATA_FAILED,

        // RTMP specific error codes
        RTMP_INVALID_FLV_FILE = 700,
        RTMP_INVALID_FLV_HEADER,
        RTMP_INVALID_STREAM_URL,
        RTMP_CONNECTION_FAILED,
        RTMP_DISCONNECTED,
        RTMP_SEND_PACKET_FAILED,

        // SpatialMedia Metadata error codes
        METADATA_INVALID = 800,
        METADATA_INJECTION_NOT_READY,
        METADATA_INJECTION_FAIL,
    }
}
