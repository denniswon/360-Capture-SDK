#pragma once

#include "EncodePacket.h"
#include "FBCaptureModule.h"

using namespace std;

namespace FBCapture {
  namespace Streaming {

  /*
  * Module for VideoEncoder and AudioEncoder that outputs encoded packet stream to EncodePacketProcessor
  * and notifies the result to FBCaptureMain
  */
    class FBCaptureEncoderDelegate : virtual public FBCaptureDelegate {
    public:
      virtual void onFinish(PACKET_TYPE type) = 0;
    };
    class EncodePacketProcessorDelegate {
    public:
      virtual ~EncodePacketProcessorDelegate() = default;
      virtual FBCAPTURE_STATUS onPacket(EncodePacket* packet) = 0;
    };

    class FBCaptureEncoderModule : public FBCaptureModule {
    public:
      FBCaptureEncoderModule(FBCaptureEncoderDelegate *mainDelegate,
                             EncodePacketProcessorDelegate *processorDelegate) :
        FBCaptureModule(mainDelegate),
        processor_(processorDelegate) {}
      virtual ~FBCaptureEncoderModule() {}

      virtual FBCAPTURE_STATUS start() override {
        const auto status = init();
        if (status != FBCAPTURE_OK)
          return status;

        if (enableAsyncMode_)
          thread_ = new thread([this] { this->run(); });

        return status;
      }

    protected:
      EncodePacketProcessorDelegate* processor_;

      virtual PACKET_TYPE type() = 0;
      virtual FBCAPTURE_STATUS getPacket(EncodePacket** packet) = 0;

      FBCAPTURE_STATUS process() override {
        EncodePacket* packet;
        auto status = getPacket(&packet);
        if (status == FBCAPTURE_ENCODER_NEED_MORE_INPUT)
          return FBCAPTURE_OK;

        if (status == FBCAPTURE_OK)
          status = processor_->onPacket(packet);
        return status;
      }

      FBCAPTURE_STATUS finish() override {
        const auto status = finalize();
        if (status != FBCAPTURE_OK)
          main_->onFailure(status);
        dynamic_cast<FBCaptureEncoderDelegate*>(main_)->onFinish(type());
        isRunning_ = false;
        return status;
      }
    };
  }
}