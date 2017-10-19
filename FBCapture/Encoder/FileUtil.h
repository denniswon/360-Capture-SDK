/****************************************************************************************************************

Filename	:	"FileUtil.h"
Content		:
Copyright	:

****************************************************************************************************************/

#pragma once

#include <stdio.h>
#include <vector>
#include <iostream>
#include <cstring>
#include <ShlObj.h>
#include <KnownFolders.h>
#include <time.h>
#include <string.h>

#define MAX_FILENAME_LENGTH 256

using namespace std;

namespace FBCapture {

  typedef const wchar_t* DestinationURL;

  typedef string fileExt;
  typedef const wchar_t* urlType;

  const fileExt kAacExt = "aac";
  const fileExt kH264Ext = "h264";
  const fileExt kFlvExt = "flv";
  const fileExt kMp4Ext = "mp4";
  const fileExt kJpgExt = "jpg";
  const fileExt kMetadataExt = "_injected.mp4";

  const urlType kRtmp = L"rtmp";
  const urlType kRtp = L"rtp";
  const urlType kHttp = L"http";
  const urlType kHttps = L"https";
  const urlType kFile = L"file";

  const bool IsStreamingUrl(DestinationURL url);
  const urlType GetURLType(DestinationURL url);
  const string GetDefaultOutputPath(const fileExt ext);
  const string ChangeFileExt(const string path, const fileExt oldExt, const fileExt newExt);

}