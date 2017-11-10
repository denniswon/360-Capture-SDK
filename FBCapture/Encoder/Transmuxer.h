#pragma once

#include "FBCaptureModule.h"
#include "FBCaptureStatus.h"
#include "metadata_utils.h"
#include "mp4muxing.h"

using namespace FBCapture::SpatialMedia;
using namespace libmp4operations;

namespace FBCapture {
  namespace Mux {

    class Transmuxer : public FBCaptureModule {
    public:
      Transmuxer(FBCaptureDelegate *mainDelegate,
                 bool enableAsyncMode);
      ~Transmuxer();

      FBCAPTURE_STATUS setInput(const string* h264FilePath,
                                const string* aacFilePath,
                                const string* mp4FilePath);

    protected:
      const string* h264FilePath_;
      const string* aacFilePath_;
      const string* mp4FilePath_;

      static const string kStitchingSoftware;

      /* FBCaptureModule */

      FBCAPTURE_STATUS init() override;
      FBCAPTURE_STATUS process() override;
      FBCAPTURE_STATUS finalize() override;
      bool continueLoop() override;
    };

  }
}