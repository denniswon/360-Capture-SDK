#pragma once
/*****************************************************************************
 *
 * Copyright 2016 Varol Okan. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ****************************************************************************/

#ifndef __METADATA_UTILS_H__
#define __METADATA_UTILS_H__

#include <stdint.h>
#include <string>
#include <fstream>
#include <map>

#include "mxml\mxml.h"
#include "mpeg\constants.h"
#include "mpeg\box.h"
#include "mpeg\sa3d.h"
#include "mpeg\mpeg4_container.h"

using namespace std;

namespace FBCapture {
  namespace SpatialMedia {

    typedef enum {
      SM_NONE, SM_TOP_BOTTOM, SM_LEFT_RIGHT
    } StereoMode;

    typedef enum {
      EQUIRECT, CUBEMAP, SINGLE_FISHEYE
    } Projection;

    // Utilities for examining/injecting spatial media metadata in MP4/MOV files."""

    static const char *MPEG_FILE_EXTENSIONS[2] = { ".mp4", ".mov" };

    static const uint8_t SPHERICAL_UUID_ID[] = { 0xff, 0xcc, 0x82, 0x63, 0xf8, 0x55, 0x4a, 0x93, 0x88, 0x14, 0x58, 0x7a, 0x02, 0x52, 0x1f, 0xdd };

    // # XML contents.
    static string RDF_PREFIX = " xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" ";

    static string SPHERICAL_XML_HEADER = "<?xml version=\"1.0\"?>"\
      "<rdf:SphericalVideo\n"\
      "xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n"\
      "xmlns:GSpherical=\"http://ns.google.com/videos/1.0/spherical/\">";

    static string SPHERICAL_XML_CONTENTS = "<GSpherical:Spherical>true</GSpherical:Spherical>"\
      "<GSpherical:Stitched>true</GSpherical:Stitched>"\
      "<GSpherical:StitchingSoftware>%s</GSpherical:StitchingSoftware>";

    static string SPHERICAL_XML_CONTENTS_EQUIRECT =
      "<GSpherical:ProjectionType>equirectangular</GSpherical:ProjectionType>";
    static string SPHERICAL_XML_CONTENTS_CUBEMAP =
      "<GSpherical:ProjectionType>cubemap</GSpherical:ProjectionType>";
    static string SPHERICAL_XML_CONTENTS_SINGLE_FISHEYE =
      "<GSpherical:ProjectionType>single_fisheye</GSpherical:ProjectionType>";

    static string SPHERICAL_XML_CONTENTS_TOP_BOTTOM = "<GSpherical:StereoMode>top-bottom</GSpherical:StereoMode>";
    static string SPHERICAL_XML_CONTENTS_LEFT_RIGHT = "<GSpherical:StereoMode>left-right</GSpherical:StereoMode>";

    // Parameter order matches that of the crop option.
    static string SPHERICAL_XML_CONTENTS_CROP_FORMAT = \
      "<GSpherical:CroppedAreaImageWidthPixels>%d</GSpherical:CroppedAreaImageWidthPixels>"\
      "<GSpherical:CroppedAreaImageHeightPixels>%d</GSpherical:CroppedAreaImageHeightPixels>"\
      "<GSpherical:FullPanoWidthPixels>%d</GSpherical:FullPanoWidthPixels>"\
      "<GSpherical:FullPanoHeightPixels>%d</GSpherical:FullPanoHeightPixels>"\
      "<GSpherical:CroppedAreaLeftPixels>%d</GSpherical:CroppedAreaLeftPixels>"\
      "<GSpherical:CroppedAreaTopPixels>%d</GSpherical:CroppedAreaTopPixels>";

    static string SPHERICAL_XML_FOOTER = "</rdf:SphericalVideo>";
    static string SPHERICAL_PREFIX = "{http://ns.google.com/videos/1.0/spherical/}";

    static AudioMetadata g_DefAudioMetadata;

    class Metadata {
    public:
      Metadata();
      virtual ~Metadata();

      void setVideoXML(string &, mxml_node_t *);
      void setVideoXML(string &);
      void setAudio(AudioMetadata *);

      string &getVideoXML();
      AudioMetadata *getAudio();

    private:
      string m_strVideoXML;
      map<string, mxml_node_t *> m_mapVideo;
      AudioMetadata *m_pAudio;
    };

    class ParsedMetadata {
    public:
      ParsedMetadata();
      virtual ~ParsedMetadata();

      // private:
      uint32_t m_iNumAudioChannels;

      typedef map<string, string> videoEntry;
      map<string, videoEntry> m_video;
      mpeg::SA3DBox *m_pAudio;
    };


    class Utils {
    public:
      Utils();
      virtual ~Utils();

      mpeg::Box *spherical_uuid(string &);
      bool mpeg4_add_spherical(mpeg::Mpeg4Container *, fstream &, string &);
      bool mpeg4_add_spatial_audio(mpeg::Mpeg4Container *, fstream &, AudioMetadata *);
      bool mpeg4_add_audio_metadata(mpeg::Mpeg4Container *, fstream &, AudioMetadata *);
      bool inject_spatial_audio_atom(fstream &, mpeg::Box *, AudioMetadata *);
      map<string, string> parse_spherical_xml(uint8_t *); // return sphericalDictionary
      ParsedMetadata *parse_spherical_mpeg4(mpeg::Mpeg4Container *, fstream &); // return metadata
      void parse_mpeg4(string &);
      bool inject_mpeg4(const string &, string &, Metadata *);
      void parse_metadata(string &);
      bool inject_metadata(const string &, string &, Metadata *);
      string &generate_spherical_xml(
        Projection,
        StereoMode,
        string,
        int *
      );
      uint8_t get_descriptor_length(fstream &);
      int32_t get_expected_num_audio_components(string &, uint32_t);
      int32_t  get_num_audio_channels(mpeg::Container *, fstream &);
      uint32_t get_sample_description_num_channels(mpeg::Container *, fstream &);
      int32_t  get_aac_num_channels(mpeg::Container *, fstream &);
      uint32_t get_num_audio_tracks(mpeg::Mpeg4Container *, fstream &);

    private:
      string m_strSphericalXML;
      map<string, string> m_mapSphericalDictionary;
    };

  }
}

#endif // __METADATA_UTILS_H__
