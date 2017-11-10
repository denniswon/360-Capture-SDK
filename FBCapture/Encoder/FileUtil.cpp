/****************************************************************************************************************

Filename	:	FileUtil.cpp
Content		:
Copyright	:

****************************************************************************************************************/

#include "FileUtil.h"
#include "Common.h"
#include "Log.h"
#include <iomanip>
#include <sstream>

#define HTON16(x)  ((x>>8&0xff)|(x<<8&0xff00))
#define HTON24(x)  ((x>>16&0xff)|(x<<16&0xff0000)|(x&0xff00))
#define HTONTIME(x) ((x>>16&0xff)|(x<<16&0xff0000)|(x&0xff00)|(x&0xff000000))
#define HTON32(x)  ((x>>24&0xff)|(x>>8&0xff00)|\
	(x<<8&0xff0000)|(x<<24&0xff000000))

namespace FBCapture {

  URL_TYPE GetUrlType(const DESTINATION_URL url) {
    wstring urlStr(url);
    auto pos = urlStr.find(L":/");
    if (pos == wstring::npos) {
      pos = urlStr.find(L":\\");
      if (pos == wstring::npos)
        return kFile;
    }

    const auto type = urlStr.substr(0, pos);
    if (type == kRtmp)
      return kRtmp;
    else if (type == kRtp)
      return kRtp;
    else if (type == kHttp)
      return kHttp;
    else if (type == kHttps)
      return kHttps;

    return kFile;
  }

  bool IsStreamingUrl(const DESTINATION_URL url) {
    wstring urlStr(url);
    auto pos = urlStr.find(L":/");
    if (pos == wstring::npos) {
      pos = urlStr.find(L":\\");
      if (pos == wstring::npos)
        return false;
    }

    const auto type = urlStr.substr(0, pos);
    if (type == kRtmp)
      return true;
    else if (type == kRtp)
      return true;
    else if (type == kHttp)
      return true;
    else if (type == kHttps)
      return true;

    return false;
  }

  string GetDefaultOutputPath(const FILE_EXT ext) {
    const LPWSTR wszPath = nullptr;
    HRESULT hr = GetCurrentDirectory(MAX_FILENAME_LENGTH, wszPath);
    if (!SUCCEEDED(hr)) return nullptr;

    wcscat(wszPath, L"\\FBCapture\\");
    if (!CreateDirectory(wszPath, nullptr) && ERROR_ALREADY_EXISTS != GetLastError())
      return nullptr;

    const auto now = chrono::system_clock::now();
    auto time = chrono::system_clock::to_time_t(now);

    wstringstream fileName;
    fileName << L"FBCapture_" << put_time(localtime(&time), L"%Y-%m-%d_%H-%M-%S") << ConvertToWide(ext);
    wcscat(wszPath, fileName.str().c_str());

    return ConvertToByte(wszPath);
  }

  string ChangeFileExt(const string path, const FILE_EXT oldExt, const FILE_EXT newExt) {
    char fileName[MAX_FILENAME_LENGTH];

    // including the . at the end right before for the file_ ext
    const auto len = path.length() - oldExt.length();

    strncpy(fileName, path.c_str(), len);
    strncpy(fileName + len, newExt.c_str(), newExt.length());
    fileName[len + newExt.length()] = '\0';

    return string(fileName);
  }

  int ReadU8(uint32_t *u8, FILE* fp) {
    if (fread(u8, 1, 1, fp) != 1)
      return 0;
    return 1;
  }

  int ReadU16(uint32_t *u16, FILE* fp) {
    if (fread(u16, 2, 1, fp) != 1)
      return 0;
    *u16 = HTON16(*u16);
    return 1;
  }

  int ReadU24(uint32_t *u24, FILE*fp) {
    if (fread(u24, 3, 1, fp) != 1)
      return 0;
    *u24 = HTON24(*u24);
    return 1;
  }

  int ReadU32(uint32_t *u32, FILE*fp) {
    if (fread(u32, 4, 1, fp) != 1)
      return 0;
    *u32 = HTON32(*u32);
    return 1;
  }

  int PeekU8(uint32_t *u8, FILE*fp) {
    if (fread(u8, 1, 1, fp) != 1)
      return 0;
    fseek(fp, -1, SEEK_CUR);
    return 1;
  }

  int ReadTime(uint32_t *utime, FILE*fp) {
    if (fread(utime, 4, 1, fp) != 1)
      return 0;
    *utime = HTONTIME(*utime);
    return 1;
  }

}
