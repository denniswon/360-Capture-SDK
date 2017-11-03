#pragma once

#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>

#include "FBCaptureStatus.h"

using namespace std;

namespace FBCapture {

  class FBCaptureDelegate {
  public:
    virtual void onFinish() = 0;
    virtual void onFailure(FBCAPTURE_STATUS status) = 0;
  };

  class FBCaptureModule {
  protected:
    thread* thread_;
    mutex mtx_;
    condition_variable cv_;
    atomic<bool> stopRequested_;
    bool isRunning_;
    bool enableAsyncMode_;  // if true, runs process() on a separate thread and
                            // delegates the result to the main asynchronously
                            // for non-blocking operation. default true.

  public:
    FBCaptureModule(FBCaptureDelegate *delegate) :
      main(delegate),
      stopRequested_(false),
      isRunning_(false),
      enableAsyncMode_(true),
      thread_(NULL) {}

    virtual ~FBCaptureModule() {
      if (thread_) {
        delete thread_;
        thread_ = NULL;
      }
    }

    virtual FBCAPTURE_STATUS start() {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;
      status = init();
      if (status != FBCAPTURE_OK)
        return status;

      if (enableAsyncMode_)
        thread_ = new thread([this] { this->run(); });
      else
        status = run();
      return status;
    }

    virtual FBCAPTURE_STATUS stop() {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;
      if (stopRequested_)
        return status;

      stopRequested_ = true;
      if (enableAsyncMode_)
        return status;

      return finish();
    }

    virtual bool join() {
      if (!isRunning_)
        return true;
      if (!stopRequested_.load())
        return false;
      if (thread_ && thread_->joinable())
        thread_->join();
      return true;
    }

    virtual bool isRunning() {
      return isRunning_;
    }

  protected:
    FBCaptureDelegate *main;

    virtual FBCAPTURE_STATUS init() = 0;
    virtual FBCAPTURE_STATUS finalize() = 0;
    virtual FBCAPTURE_STATUS process() = 0;

    virtual bool continueLoop() {
      return true;
    }

    virtual FBCAPTURE_STATUS finish() {
      FBCAPTURE_STATUS status = finalize();
      if (status != FBCAPTURE_OK)
        main->onFailure(status);
      main->onFinish();
      isRunning_ = false;
      return status;
    }

    virtual FBCAPTURE_STATUS run() {
      FBCAPTURE_STATUS status = FBCAPTURE_OK;
      isRunning_ = true;

      bool loop = !stopRequested_.load();
      while (loop) {
        if (stopRequested_.load())
          break;
        status = process();
        if (status != FBCAPTURE_OK) {
          main->onFailure(status);
          return status;
        }
        loop = continueLoop();
      }
      return finish();
    }
  };
}