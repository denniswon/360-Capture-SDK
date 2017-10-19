/****************************************************************************************************************

Filename	:	Log.cpp
Content		:
Copyright	:

****************************************************************************************************************/

#include "Log.h"

namespace FBCapture {

  const string Log::kLog = "[LOG]";
  const string Log::kError = "[ERROR]";
  const char* Log::kLogFile = "FBCaptureSDK.txt";
  Log* Log::kInstance = nullptr;
  mutex Log::mutex_;

  Log& Log::instance() {
    lock_guard<mutex> lock(mutex_);
    if (kInstance == nullptr)
      kInstance = new Log();
    return *kInstance;
  }

  void Log::release() {
    lock_guard<mutex> lock(Log::mutex_);
    delete Log::kInstance;
    Log::kInstance = nullptr;
  }

  Log::~Log() {
    output_.close();
  }

  Log::Log() {
    output_.open(kLogFile, ios_base::out);
    if (!output_.good()) {
      throw runtime_error("Initialization is failed");
    }
  }

  void Log::log(const string& log, const string& logType) {
    lock_guard<mutex> lock(mutex_);
    logWriter(log, logType);
  }

  void Log::log(const string& log, const string& var, const string& logType) {
    lock_guard<mutex> lock(mutex_);
    logWriter(log, var, logType);
  }

  void Log::logWriter(const string& log, const string& logType) {
    output_ << getCurrentTime() << logType << ": " << log << endl;
  }

  void Log::logWriter(const string& log, const string& var, const string& logType) {
    output_ << getCurrentTime() << logType << ": " << log << ": " << var << endl;
  }

  string Log::getCurrentTime() {
    auto now = chrono::system_clock::now();
    auto time = chrono::system_clock::to_time_t(now);

    stringstream timeStamp;
    timeStamp << put_time(localtime(&time), "[ %Y-%m-%d %X ]");
    return timeStamp.str();
  }

}
