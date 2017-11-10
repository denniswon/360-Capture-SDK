#pragma once

#include <comdef.h>
#include <codecvt>

using namespace std;

#define CHECK_HR(hr) if (hr != S_OK) { return hr; }
#define CHECK_HR_EXIT(hr) if (hr != S_OK) { goto exit; }
#define CHECK_HR_STATUS(hr, failStatus) if (hr != S_OK) { return failStatus; }
#define CHECK_MMR_EXIT(mmr, msg) if (mmr != MMSYSERR_NOERROR) { goto exit; }
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(a) if (a) { a->Release(); a = NULL; }
#endif

namespace FBCapture {

  inline void ConvertToByte(wchar_t* in, char** out) {
    if (!in) return;
    const auto size = wcslen(in) * 2 + 2;
    *out = new char[size];
    size_t cSize;
    wcstombs_s(&cSize, *out, size, in, size);
  }

  inline string ConvertToByte(const wstring path) {
    wstring_convert<codecvt_utf8<wchar_t>> stringTypeConversion;
    return stringTypeConversion.to_bytes(path).c_str();
  }

  inline void ConvertToWide(char* in, wchar_t** out) {
    if (!in) return;
    const auto size = strlen(in) + 1;
    *out = new wchar_t[size];
    size_t c_size;
    mbstowcs_s(&c_size, *out, size, in, size);
  }

  inline wstring ConvertToWide(const string path) {
    wstring_convert<codecvt_utf8<wchar_t>> stringTypeConversion;
    return stringTypeConversion.from_bytes(path).c_str();
  }

}
