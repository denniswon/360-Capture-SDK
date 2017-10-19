/****************************************************************************************************************

Filename	:	FileUtil.cpp
Content		:
Copyright	:

****************************************************************************************************************/

#include "FileUtil.h"
#include "Common.h"
#include "Log.h"

namespace FBCapture {

  const urlType GetURLType(DestinationURL url) {
    wstring urlStr(url);
    size_t pos = urlStr.find(L":/");
    if (pos == wstring::npos) {
      pos = urlStr.find(L":\\");
      if (pos == wstring::npos)
        return kFile;
    }

    auto type = urlStr.substr(0, pos);
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

  const bool IsStreamingUrl(DestinationURL url) {
    wstring urlStr(url);
    size_t pos = urlStr.find(L":/");
    if (pos == wstring::npos) {
      pos = urlStr.find(L":\\");
      if (pos == wstring::npos)
        return false;
    }

    auto type = urlStr.substr(0, pos);
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

  const string GetDefaultOutputPath(const fileExt ext) {
    LPWSTR wszPath = NULL;
    HRESULT hr = GetCurrentDirectory(MAX_FILENAME_LENGTH, wszPath);
    if (!SUCCEEDED(hr)) return NULL;

    wcscat(wszPath, L"\\FBCapture\\");
    if (!CreateDirectory(wszPath, NULL) && ERROR_ALREADY_EXISTS != GetLastError())
      return NULL;

    auto now = chrono::system_clock::now();
    auto time = chrono::system_clock::to_time_t(now);

    wstringstream filename;
    filename << L"FBCapture_" << put_time(localtime(&time), L"%Y-%m-%d_%H-%M-%S") << ConvertToWide(ext);
    wcscat(wszPath, filename.str().c_str());

    return ConvertToByte(wszPath);
  }

  const string ChangeFileExt(const string path, const fileExt oldExt, const fileExt newExt) {
    char filename[MAX_FILENAME_LENGTH];

    // including the . at the end right before for the file ext
    size_t len = path.length() - oldExt.length();

    strncpy(filename, path.c_str(), len);
    strncpy(filename + len, newExt.c_str(), newExt.length());
    filename[len + newExt.length()] = '\0';

    return string(filename);
  }

}
