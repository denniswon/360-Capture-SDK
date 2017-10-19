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
      virtual void onFinish(PacketType type) = 0;
    };
    class EncodePacketProcessorDelegate {
    public:
      virtual FBCAPTURE_STATUS onPacket(EncodePacket* packet) = 0;
    };

    class FBCaptureEncoderModule : public FBCaptureModule {
    public:
      FBCaptureEncoderModule(FBCaptureEncoderDelegate *mainDelegate,
                             EncodePacketProcessorDelegate *processorDelegate) :
        FBCaptureModule(mainDelegate),
        processor(processorDelegate) {}
      virtual ~FBCaptureEncoderModule() {}

      virtual FBCAPTURE_STATUS start() override {
        FBCAPTURE_STATUS status = FBCAPTURE_OK;
        status = init();
        if (status != FBCAPTURE_OK)
          return status;

        if (enableAsyncMode_)
          thread_ = new thread([this] { this->run(); });

        return status;
      }

    protected:
      EncodePacketProcessorDelegate* processor;

      virtual const PacketType type() = 0;
      virtual FBCAPTURE_STATUS getPacket(EncodePacket** packet) = 0;

      virtual FBCAPTURE_STATUS process() {
        EncodePacket* packet;
        FBCAPTURE_STATUS status = getPacket(&packet);
        if (status == FBCAPTURE_ENCODER_NEED_MORE_INPUT)
          return FBCAPTURE_OK;

        if (status == FBCAPTURE_OK)
          status = processor->onPacket(packet);
        return status;
      }

      virtual void finish() override {
        dynamic_cast<FBCaptureEncoderDelegate*>(main)->onFinish(type());
        done();
      }
    };
  }
}