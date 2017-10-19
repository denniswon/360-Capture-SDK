/****************************************************************************************************************

Filename	:	Log.h
Content		:
Copyright	:

****************************************************************************************************************/

#pragma once
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <mutex>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <time.h>

using namespace std;
#define DEBUG_LOG(message) \
	FBCapture::Log::instance().log(message, FBCapture::Log::kLog)

#define DEBUG_LOG_VAR(message, vars) \
	FBCapture::Log::instance().log(message, vars, FBCapture::Log::kLog)

#define DEBUG_ERROR(message) \
	FBCapture::Log::instance().log(message, FBCapture::Log::kError)

#define DEBUG_ERROR_VAR(message, vars) \
	FBCapture::Log::instance().log(message, vars, FBCapture::Log::kError)

#define RELEASE_LOG() \
	FBCapture::Log::instance().release();

namespace FBCapture {

  class Log {
  private:
    Log();
    virtual ~Log();
    Log(const Log&);
    static mutex mutex_;

  public:
    static const string kLog;
    static const string kError;

    static Log& instance();

    void log(const string& log, const string& logType);
    void log(const string& log, const string& var, const string& logType);
    void release();

    static string getCurrentTime();

  private:
    static Log* kInstance;
    static const char* kLogFile;
    ofstream output_;

    void logWriter(const string& log, const string& logType);
    void logWriter(const string& log, const string& var, const string& inLogLevel);
  };

}
