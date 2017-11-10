#pragma once

#include <atomic>

using namespace std;

namespace FBCapture {

  struct FrameCounter {
    atomic<int> count;

    FrameCounter() : count(0) {}
    explicit FrameCounter(const int cnt) : count(cnt) {}

    void increment() {
      ++count;
    }

    void reset() {
      count = 0;
    }

    int get() const {
      return count.load();
    }
  };

}