/****************************************************************************************************************

Filename	:	Transmuxer.cpp
Content		:
Copyright	:

****************************************************************************************************************/

#include "Transmuxer.h"
#include "FileUtil.h"
#include "Log.h"

#define APPEND_METADATA_SUFFIX 1

namespace FBCapture {
  namespace Mux {

    const string Transmuxer::kStitchingSoftware = "Facebook 360 Capture SDK";

    Transmuxer::Transmuxer(FBCaptureDelegate *mainDelegate,
                           bool enableAsyncMode) :
      FBCaptureModule(mainDelegate),
      h264FilePath_(NULL),
      aacFilePath_(NULL),
      mp4FilePath_(NULL) {
      enableAsyncMode_ = enableAsyncMode;
    }

    Transmuxer::~Transmuxer() {
      finalize();
    }

    FBCAPTURE_STATUS Transmuxer::setInput(const string* h264FilePath,
                                          const string* aacFilePath,
                                          const string* mp4FilePath) {
      h264FilePath_ = h264FilePath;
      aacFilePath_ = aacFilePath;
      mp4FilePath_ = mp4FilePath;
      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS Transmuxer::init() {
      return FBCAPTURE_OK;
    }

    FBCAPTURE_STATUS Transmuxer::process() {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;

      // A/V muxing h264 and aac to mp4
      uint32_t nErrorCode = mp4muxAVStreams(
        (*h264FilePath_).c_str(),
        (*aacFilePath_).c_str(),
        (*mp4FilePath_).c_str(),
        0.0f,
        0.0f,
        NO_DURATION_SPECIFIED,
        UNKNOWN_FRAMES_PER_SECOND,
        (eVideoRotationMode)VIDEO_ROTATION_MODE_NO_ROTATION,
        false);

      if (0 != nErrorCode) {
        DEBUG_ERROR_VAR("Failed muxing A/V stream", to_string(nErrorCode));
        status = FBCAPTURE_WAMDEIA_MUXING_FAILED;
        return status;
      }

      // After muxing, inject spherical video metadata
      Utils utils;
      Metadata md;
      string &strVideoXML = utils.generate_spherical_xml(
        Projection::EQUIRECT,
        StereoMode::SM_NONE,
        kStitchingSoftware,
        NULL
      );

      if (strVideoXML.length() <= 1) {
        DEBUG_ERROR("Failed to generate spherical video metadata.");
        status = FBCAPTURE_METADATA_INVALID;
        return status;
      }
      md.setVideoXML(strVideoXML);

      string outputFile = APPEND_METADATA_SUFFIX
        ? ChangeFileExt(*mp4FilePath_, kMp4Ext, kMetadataExt)
        : *mp4FilePath_;

      if (!utils.inject_metadata(*mp4FilePath_, outputFile, &md)) {
        DEBUG_ERROR("Failed injecting spherical video metadata.");
        status = FBCAPTURE_METADATA_INJECTION_FAIL;
      }

      return status;
    }

    FBCAPTURE_STATUS Transmuxer::finalize() {
      // Remove input h264, aac files after muxing is done
      remove((*h264FilePath_).c_str());
      h264FilePath_ = NULL;
      remove((*aacFilePath_).c_str());
      aacFilePath_ = NULL;

      // if metadata injecteion creates a new file, remove the input mp4 file
      if (APPEND_METADATA_SUFFIX)
        remove((*mp4FilePath_).c_str());
      mp4FilePath_ = NULL;
      return FBCAPTURE_OK;
    }

    bool Transmuxer::continueLoop() {
      return false;
    }
  }
}