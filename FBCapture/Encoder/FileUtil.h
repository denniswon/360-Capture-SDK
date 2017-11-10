/****************************************************************************************************************

Filename	:	"FileUtil.h"
Content		:
Copyright	:

****************************************************************************************************************/

#pragma once

#include <iostream>

#define MAX_FILENAME_LENGTH 256

#define OPEN_FILE(file_, path) \
errno_t err = fopen_s(&file_, path.c_str(), "wb"); \
if (err) { \
  DEBUG_ERROR_VAR("Failed opening file_", path); \
  return FBCAPTURE_OUTPUT_FILE_OPEN_FAILED; \
}
#define CLOSE_FILE(file_) if (file_) { fclose(file_); file_ = NULL; }
#define REMOVE_FILE(file_PathPtr) \
if (file_PathPtr) { \
  remove((*file_PathPtr).c_str());\
  delete file_PathPtr; \
  file_PathPtr = NULL; \
}

using namespace std;

namespace FBCapture {

  typedef const wchar_t* DESTINATION_URL;
  typedef string FILE_EXT;
  typedef const wchar_t* URL_TYPE;

  const FILE_EXT kAacExt = "aac";
  const FILE_EXT kH264Ext = "h264";
  const FILE_EXT kFlvExt = "flv";
  const FILE_EXT kMp4Ext = "mp4";
  const FILE_EXT kJpgExt = "jpg";
  const FILE_EXT kMetadataExt = "_injected.mp4";

  const URL_TYPE kRtmp = L"rtmp";
  const URL_TYPE kRtp = L"rtp";
  const URL_TYPE kHttp = L"http";
  const URL_TYPE kHttps = L"https";
  const URL_TYPE kFile = L"file_";

  bool IsStreamingUrl(DESTINATION_URL url);
  URL_TYPE GetUrlType(DESTINATION_URL url);
  string GetDefaultOutputPath(const FILE_EXT ext);
  string ChangeFileExt(const string path, const FILE_EXT oldExt, const FILE_EXT newExt);

  int ReadU8(uint32_t *u8, FILE* fp);      // Read 1 uint8_t
  int ReadU16(uint32_t *u8, FILE* fp);     // Read 2 uint8_t
  int ReadU24(uint32_t *u8, FILE* fp);     // Read 3 uint8_t
  int ReadU32(uint32_t *u32, FILE*fp);     // Read 4 uint8_t
  int PeekU8(uint32_t *u8, FILE*fp);       // Read 1 uint8_t and loopback 1 uint8_t
  int ReadTime(uint32_t *utime, FILE*fp);  // Read 4 bytes and convert to time format

}