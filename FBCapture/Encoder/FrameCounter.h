#pragma once

#include <atomic>

using namespace std;

namespace FBCapture {

  struct FrameCounter {
    atomic<int> count;

    FrameCounter() : count(0) {}
    FrameCounter(int cnt) : count(cnt) {}

    void increment() {
      ++count;
    }

    void reset() {
      count = 0;
    }

    int get() {
      return count.load();
    }
  };

}