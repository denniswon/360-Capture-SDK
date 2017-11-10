/****************************************************************************************************************

Filename	:	Log.cpp
Content		:
Copyright	:

****************************************************************************************************************/

#include "Log.h"
#include <sstream>
#include <iomanip>

namespace FBCapture {

  const string Log::kLog = "[LOG]";
  const string Log::kError = "[ERROR]";
  const char* Log::logFile = "FBCaptureSDK.txt";
  Log* Log::singleton = nullptr;
  mutex Log::mtx;

  Log& Log::instance() {
    lock_guard<mutex> lock(mtx);
    if (singleton == nullptr)
      singleton = new Log();
    return *singleton;
  }

  void Log::release() {
    lock_guard<mutex> lock(Log::mtx);
    delete Log::singleton;
    Log::singleton = nullptr;
  }

  Log::~Log() {
    output_.close();
  }

  Log::Log() {
    output_.open(logFile, ios_base::out);
    if (!output_.good()) {
      throw runtime_error("Initialization is failed");
    }
  }

  void Log::log(const string& log, const string& logType) {
    lock_guard<mutex> lock(mtx);
    logWriter(log, logType);
  }

  void Log::log(const string& log, const string& var, const string& logType) {
    lock_guard<mutex> lock(mtx);
    logWriter(log, var, logType);
  }

  void Log::logWriter(const string& log, const string& logType) {
    output_ << getCurrentTime() << logType << ": " << log << endl;
  }

  void Log::logWriter(const string& log, const string& var, const string& logType) {
    output_ << getCurrentTime() << logType << ": " << log << ": " << var << endl;
  }

  string Log::getCurrentTime() {
    const auto now = chrono::system_clock::now();
    auto time = chrono::system_clock::to_time_t(now);

    stringstream timeStamp;
    timeStamp << put_time(localtime(&time), "[ %Y-%m-%d %X ]");
    return timeStamp.str();
  }

}
