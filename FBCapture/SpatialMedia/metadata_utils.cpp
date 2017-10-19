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

#include <iostream>
#include <sstream>

#include "mpeg/constants.h"
#include "mpeg/mpeg4_container.h"
#include "mpeg/sa3d.h"

#include "metadata_utils.h"

namespace FBCapture {
  namespace SpatialMedia {

    const char *SPHERICAL_TAGS_LIST[] = {
      "Spherical",
      "Stitched",
      "StitchingSoftware",
      "ProjectionType",
      "SourceCount",
      "StereoMode",
      "InitialViewHeadingDegrees",
      "InitialViewPitchDegrees",
      "InitialViewRollDegrees",
      "Timestamp",
      "CroppedAreaImageWidthPixels",
      "CroppedAreaImageHeightPixels",
      "FullPanoWidthPixels",
      "FullPanoHeightPixels",
      "CroppedAreaLeftPixels",
      "CroppedAreaTopPixels",
    };

    Metadata::Metadata() {
      m_pAudio = NULL;
    }

    Metadata::~Metadata() {}

    void Metadata::setVideoXML(string &str, mxml_node_t *pNode) {
      m_mapVideo[str] = pNode;
    }

    void Metadata::setVideoXML(string &str) {
      m_strVideoXML = str;
    }

    void Metadata::setAudio(AudioMetadata *pAudio) {
      m_pAudio = pAudio;
    }

    string &Metadata::getVideoXML() {
      return m_strVideoXML;
    }

    AudioMetadata *Metadata::getAudio() {
      return m_pAudio;
    }

    ParsedMetadata::ParsedMetadata() {
      m_pAudio = NULL;
      m_iNumAudioChannels = 0;
    }

    ParsedMetadata::~ParsedMetadata() {}

    Utils::Utils() {}

    Utils::~Utils() {}

    mpeg::Box *Utils::spherical_uuid(string &strMetadata) {
      // Constructs a uuid containing spherical metadata.
      mpeg::Box *p = new mpeg::Box;
      // a box containing spherical metadata.
      //  if ( strUUID.length ( ) != 16 )
      //    cerr << "ERROR: Data mismatch" << endl;
      int iSize = (int)strMetadata.size();
      const uint8_t *pMetadata = reinterpret_cast<const uint8_t*>(strMetadata.c_str());

      memcpy(p->m_name, mpeg::constants::TAG_UUID, 4);
      p->m_iHeaderSize = 8;
      p->m_iContentSize = 0;
      p->m_pContents = new uint8_t[iSize + 16 + 1];
      memcpy(p->m_pContents, SPHERICAL_UUID_ID, 16);
      memcpy((p->m_pContents + 16), pMetadata, iSize);
      p->m_iContentSize = iSize + 16;

      return p;
    }

    bool Utils::mpeg4_add_spherical(mpeg::Mpeg4Container *pMPEG4, fstream &inFile, string &strMetadata) {
      // Adds a spherical uuid box to an mpeg4 file for all video tracks.
      //
      // pMPEG4 : Mpeg4 file structure to add metadata.
      // inFile : file handle, Source for uncached file contents.
      // strMetadata: string, xml metadata to inject into spherical tag.
      if (!pMPEG4)
        return false;

      bool bAdded = false;
      mpeg::Container *pMoov = (mpeg::Container *)pMPEG4->m_pMoovBox;
      if (!pMoov)
        return false;

      vector<mpeg::Box *>::iterator it = pMoov->m_listContents.begin();
      while (it != pMoov->m_listContents.end()) {
        mpeg::Container *pBox = (mpeg::Container *)*it++;
        if (memcmp(pBox->m_name, mpeg::constants::TAG_TRAK, 4) == 0) {
          bAdded = false;
          pBox->remove(mpeg::constants::TAG_UUID);

          vector<mpeg::Box *>::iterator it2 = pBox->m_listContents.begin();
          while (it2 != pBox->m_listContents.end()) {
            mpeg::Container *pSub = (mpeg::Container *)*it2++;
            if (memcmp(pSub->m_name, mpeg::constants::TAG_MDIA, 4) != 0)
              continue;

            vector<mpeg::Box *>::iterator it3 = pSub->m_listContents.begin();
            while (it3 != pSub->m_listContents.end()) {
              mpeg::Box *pMDIA = *it3++;
              if (memcmp(pMDIA->m_name, mpeg::constants::TAG_HDLR, 4) != 0)
                continue;

              char name[4];
              int iPos = pMDIA->content_start() + 8;
              inFile.seekg(iPos);
              inFile.read(name, 4);
              if (memcmp(name, mpeg::constants::TRAK_TYPE_VIDE, 4) == 0) {
                bAdded = true;
                break;
              }
            }
            if (bAdded) {
              if (!pBox->add(spherical_uuid(strMetadata)))
                return true;
              break;
            }
          }
        }
      }
      pMPEG4->resize();
      return true;
    }

    bool Utils::mpeg4_add_spatial_audio(mpeg::Mpeg4Container *pMPEG4, fstream &inFile, AudioMetadata *pAudio) {
      // pMPEG4 is Mpeg4 file structure to add metadata.
      // inFile: file handle, Source for uncached file contents.
      // pAudio: dictionary ('ambisonic_type': string, 'ambisonic_order': int),
      //                      Supports 'periphonic' ambisonic type only.
      if (!pMPEG4)
        return false;

      mpeg::Container *pMoov = (mpeg::Container *)pMPEG4->m_pMoovBox;
      if (!pMoov)
        return false;

      vector<mpeg::Box *>::iterator it = pMoov->m_listContents.begin();
      while (it != pMoov->m_listContents.end()) {
        mpeg::Container *pBox = (mpeg::Container *)*it++;
        if (memcmp(pBox->m_name, mpeg::constants::TAG_TRAK, 4) != 0)
          continue;

        vector<mpeg::Box *>::iterator it2 = pBox->m_listContents.begin();
        while (it2 != pBox->m_listContents.end()) {
          mpeg::Container *pSub = (mpeg::Container *)*it2++;
          if (memcmp(pSub->m_name, mpeg::constants::TAG_MDIA, 4) != 0)
            continue;

          vector<mpeg::Box *>::iterator it3 = pSub->m_listContents.begin();
          while (it3 != pSub->m_listContents.end()) {
            mpeg::Box *pMDIA = *it3++;
            if (memcmp(pMDIA->m_name, mpeg::constants::TAG_HDLR, 4) != 0)
              continue;

            char name[4];
            int iPos = pMDIA->content_start() + 8;
            inFile.seekg(iPos);
            inFile.read(name, 4);
            if (memcmp(name, mpeg::constants::TAG_SOUN, 4) == 0)
              return inject_spatial_audio_atom(inFile, pSub, pAudio);
          }
        }
      }
      return true;
    }

    bool Utils::mpeg4_add_audio_metadata(mpeg::Mpeg4Container *pMPEG4, fstream &inFile, AudioMetadata *pAudio) {
      int num_audio_tracks = get_num_audio_tracks(pMPEG4, inFile);
      if (num_audio_tracks > 1) {
        cerr << "Error: Expected 1 audio track. Found " << num_audio_tracks << endl;
        return false;
      }
      return mpeg4_add_spatial_audio(pMPEG4, inFile, pAudio);
    }

    bool inArray(char *pName, const char **ppArray, int iSize) {
      if (pName == NULL)
        return false;
      for (int t = 0; t < iSize; t++) {
        if (memcmp(pName, ppArray[t], 4) == 0)
          return true;
      }

      return false;
    }

    bool Utils::inject_spatial_audio_atom(fstream &inFile, mpeg::Box *pAudioMediaAtom, AudioMetadata *pAudio) {
      if (!pAudioMediaAtom || !pAudio)
        return false;

      int iArraySize = sizeof(mpeg::constants::SOUND_SAMPLE_DESCRIPTIONS);
      mpeg::Container *pMediaAtom = (mpeg::Container *)pAudioMediaAtom;

      vector<mpeg::Box *>::iterator it = pMediaAtom->m_listContents.begin();
      while (it != pMediaAtom->m_listContents.end()) {
        mpeg::Container *pAtom = (mpeg::Container *)*it++;
        if (memcmp(pAtom->m_name, mpeg::constants::TAG_MINF, 4) != 0)
          continue;

        vector<mpeg::Box *>::iterator it2 = pAtom->m_listContents.begin();
        while (it2 != pAtom->m_listContents.end()) {
          mpeg::Container *pElement = (mpeg::Container *)*it2++;
          if (memcmp(pElement->m_name, mpeg::constants::TAG_STBL, 4) != 0)
            continue;

          vector<mpeg::Box *>::iterator it3 = pElement->m_listContents.begin();
          while (it3 != pElement->m_listContents.end()) {
            mpeg::Container *pSub = (mpeg::Container *)*it3++;
            if (memcmp(pSub->m_name, mpeg::constants::TAG_STSD, 4) != 0)
              continue;

            vector<mpeg::Box *>::iterator it4 = pSub->m_listContents.begin();
            while (it4 != pSub->m_listContents.end()) {
              mpeg::Container *pSample = (mpeg::Container *)*it4++;
              if (inArray(pSample->m_name, mpeg::constants::SOUND_SAMPLE_DESCRIPTIONS, iArraySize)) {
                inFile.seekg(pSample->m_iPosition + pSample->m_iHeaderSize + 16);
                string strAmbisonicType = pAudio->ambisonic_type;
                uint32_t iAmbisonicOrder = pAudio->ambisonic_order;
                int iNumChannels = get_num_audio_channels(pSub, inFile);
                int iNumAmbisonicComponents = get_expected_num_audio_components(strAmbisonicType, iAmbisonicOrder);
                if (iNumChannels != iNumAmbisonicComponents) {
                  cerr << "Error: Found " << iNumChannels << " audio channel(s). ";
                  cerr << "Expected " << iNumAmbisonicComponents << " chanel(s) for ";
                  cerr << strAmbisonicType << " ambisonics of order " << iAmbisonicOrder << "." << endl;
                  return false;
                }
                mpeg::Box *pSA3DAtom = mpeg::SA3DBox::create(iNumChannels, *pAudio);
                pSample->m_listContents.push_back(pSA3DAtom);
              }
            }
          }
        }
      }
      return true;
    }

    map<string, string> Utils::parse_spherical_xml(uint8_t *pContents) // return sphericalDictionary
    {
      // Returns  spherical metadata for a set of xml data.
      // Args:    string, spherical metadata xml contents.
      // Retruns: dictionary containing the parsed spherical metadata values.
      char *ptr = NULL;
      string strContents;
      mxml_node_t *parsed_xml = mxmlLoadString(NULL, (char *)pContents, MXML_OPAQUE_CALLBACK); // MXML_NO_CALLBACK );
      if (!parsed_xml) {
        cerr << "\t\tParser Error on XML. " << (char *)pContents << endl;
        return m_mapSphericalDictionary;
      }
      // mxmlSaveString(tree, buffer, sizeof(buffer), MXML_NO_CALLBACK);
      // ptr = mxmlSaveAllocString(tree, MXML_NO_CALLBACK);
      mxml_node_t *pNode = mxmlFindElement(parsed_xml, parsed_xml, "rdf:SphericalVideo", "xmlns:rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#", MXML_DESCEND);
      if (!pNode) {
        pNode = mxmlFindElement(parsed_xml, parsed_xml, "rdf:SphericalVideo", NULL, NULL, MXML_DESCEND);
        if (!pNode) {
          cerr << "\t\tError could not find tag : \"rdf:SphericalVideo\"" << endl;
          return m_mapSphericalDictionary;
        }
        mxmlElementSetAttr(pNode, "xmlns:rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#");
      }

      int iArraySize = (int)(sizeof(SPHERICAL_TAGS_LIST) / sizeof(SPHERICAL_TAGS_LIST[0]));
      string strText;
      mxml_node_t *pChild = mxmlGetFirstChild(pNode);
      while (pChild != NULL) {
        const char *pTag = mxmlGetElement(pChild);
        const char *pText = mxmlGetOpaque(pChild);
        if (pTag != NULL) {
          const char *p = strchr(pTag, ':');
          if (p)
            pTag = p + 1;
          if (inArray((char *)pTag, SPHERICAL_TAGS_LIST, iArraySize)) {
            strText = "";
            if (pText)
              strText = pText;
            cout << "\t\t" << pTag << " = " << strText << endl;
            m_mapSphericalDictionary[pTag] = strText;
          } else {
            cout << "\t\tUnknown: " << pTag << " = " << strText << endl;
            // tag = child.tag
            //   if child.tag[:len(spherical_prefix)] == spherical_prefix:
            //      tag = child.tag[len(spherical_prefix):]
          }
        }
        pChild = mxmlGetNextSibling(pChild);
      }

      return m_mapSphericalDictionary;
    }

    ParsedMetadata *Utils::parse_spherical_mpeg4(mpeg::Mpeg4Container *pMPEG4, fstream &file) // return metadata
    {
      // pMPEG4 is Mpeg4 file structure to add metadata.
      // file: handle, Source for uncached file contents.
      if (!pMPEG4)
        return NULL;

      int iTrackNum = 0;
      int iArraySize = (int)(sizeof(mpeg::constants::SOUND_SAMPLE_DESCRIPTIONS) / sizeof(mpeg::constants::SOUND_SAMPLE_DESCRIPTIONS[0]));
      uint8_t  buffer[16];
      char    *pNewedBuffer = NULL;
      uint8_t *pSubElementID = NULL; // pointing to array of bytes
      uint8_t *pContents = NULL;


      mpeg::Container *pMoov = (mpeg::Container *)pMPEG4->m_pMoovBox;
      if (!pMoov)
        return NULL;

      ParsedMetadata *pMetadata = new ParsedMetadata;
      vector<mpeg::Box *>::iterator it = pMoov->m_listContents.begin();
      while (it != pMoov->m_listContents.end()) {
        mpeg::Container *pBox = (mpeg::Container *)*it++;
        if (memcmp(pBox->m_name, mpeg::constants::TAG_TRAK, 4) != 0)
          continue;

        stringstream ss;
        ss << "Track " << iTrackNum++;
        string trackName = ss.str();
        cout << "\t" << trackName << endl;

        vector<mpeg::Box *>::iterator it2 = pBox->m_listContents.begin();
        while (it2 != pBox->m_listContents.end()) {
          mpeg::Container *pSub = (mpeg::Container *)*it2++;
          if (memcmp(pSub->m_name, mpeg::constants::TAG_UUID, 4) == 0) {
            if (pSub->m_pContents)
              pSubElementID = pSub->m_pContents;
            else {
              file.seekg(pSub->content_start());
              file.read((char *)buffer, 16);
              pSubElementID = buffer;
            }
            if (memcmp(pSubElementID, SPHERICAL_UUID_ID, 16) == 0) {
              if (pSub->m_pContents)
                pContents = &pSub->m_pContents[16];
              else {
                // fh.read(sub_element.content_size - 16)
                pNewedBuffer = new  char[pSub->m_iContentSize - 16];
                file.read(pNewedBuffer, pSub->m_iContentSize - 16);
                pContents = (uint8_t *)pNewedBuffer;
              }
              // metadata.video[trackName] = parse_spherical_xml(contents, console)
              pMetadata->m_video[trackName] = parse_spherical_xml(pContents);
            }
          }

          // Note: I am not sure if the Python script has a bug here wrt sub_element vs element
          //       The indentation is off !
          // for element in mpeg4_file.moov_box.contents:
          //   if element.name == mpeg.constants.TAG_TRAK:
          //     for sub_element in element.contents:
          //       if sub_element.name == mpeg.constants.TAG_UUID:
          //         ...
          //     if sub_element.name == mpeg.constants.TAG_MDIA:
          //       ...
          if (memcmp(pSub->m_name, mpeg::constants::TAG_MDIA, 4) == 0) {
            vector<mpeg::Box *>::iterator it3 = pSub->m_listContents.begin();
            while (it3 != pSub->m_listContents.end()) {
              mpeg::Container *pMDIA = (mpeg::Container *)*it3++;
              if (memcmp(pMDIA->m_name, mpeg::constants::TAG_MINF, 4) != 0)
                continue;

              vector<mpeg::Box *>::iterator it4 = pMDIA->m_listContents.begin();
              while (it4 != pMDIA->m_listContents.end()) {
                mpeg::Container *pSTBL = (mpeg::Container *)*it4++;
                if (memcmp(pSTBL->m_name, mpeg::constants::TAG_STBL, 4) != 0)
                  continue;

                vector<mpeg::Box *>::iterator it5 = pSTBL->m_listContents.begin();
                while (it5 != pSTBL->m_listContents.end()) {
                  mpeg::Container *pSTSD = (mpeg::Container *)*it5++;
                  if (memcmp(pSTSD->m_name, mpeg::constants::TAG_STSD, 4) != 0)
                    continue;

                  vector<mpeg::Box *>::iterator it6 = pSTSD->m_listContents.begin();
                  while (it6 != pSTSD->m_listContents.end()) {
                    mpeg::Container *pSA3D = (mpeg::Container *)*it6++;
                    if (!inArray(pSA3D->m_name, mpeg::constants::SOUND_SAMPLE_DESCRIPTIONS, iArraySize))
                      continue;

                    pMetadata->m_iNumAudioChannels = get_num_audio_channels(pSTSD, file);
                    vector<mpeg::Box *>::iterator it7 = pSA3D->m_listContents.begin();
                    while (it7 != pSA3D->m_listContents.end()) {
                      mpeg::Container *pItem = (mpeg::Container *)*it7++;
                      if (memcmp(pItem->m_name, mpeg::constants::TAG_SA3D, 4) == 0) {
                        mpeg::SA3DBox *pSA = (mpeg::SA3DBox *)pItem;
                        pSA->print_box();
                        pMetadata->m_pAudio = new mpeg::SA3DBox(*pSA);
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      if (pNewedBuffer)
        delete[]pNewedBuffer;

      return pMetadata;
    }

    void Utils::parse_mpeg4(string &strFileName) {
      fstream file(strFileName.c_str(), ios::in | ios::binary | ios::ate);
      if (!file.is_open()) {
        cerr << "Error \"" << strFileName << "\" does not exist or do not have permission." << endl;
        return;
      }
      mpeg::Mpeg4Container *pMPEG4 = mpeg::Mpeg4Container::load(file);
      if (!pMPEG4) {
        cerr << "Error, file could not be opened." << endl;
        return;
      }
      cout << "File loaded." << endl;
      parse_spherical_mpeg4(pMPEG4, file);
    }

    bool Utils::inject_mpeg4(const string &strInFile, string &strOutFile, Metadata *pMetadata) {
      fstream inFile(strInFile.c_str(), ios::in | ios::binary | ios::ate);
      if (!inFile.is_open()) {
        cerr << "Error \"" << strInFile << "\" does not exist or do not have permission." << endl;
        return false;
      }
      mpeg::Mpeg4Container *pMPEG4 = mpeg::Mpeg4Container::load(inFile);
      if (!pMPEG4) {
        cerr << "Error, file could not be opened." << endl;
        return false;
      }
      bool bRet = mpeg4_add_spherical(pMPEG4, inFile, pMetadata->getVideoXML());
      if (!bRet) {
        cerr << "Error failed to insert spherical data" << endl;
        return false;
      }
      if (pMetadata->getAudio()) {
        bRet = mpeg4_add_audio_metadata(pMPEG4, inFile, pMetadata->getAudio());
        if (!bRet) {
          cerr << "Error failed to insert spatial audio data" << endl;
          return false;
        }
      }
      cout << "Saved file settings" << endl;
      parse_spherical_mpeg4(pMPEG4, inFile);

      fstream outFile(strOutFile.c_str(), ios::out | ios::binary);
      if (!outFile.is_open()) {
        cerr << "Error file: \"" << strOutFile << "\" could not create or do not have permission." << endl;
        return false;
      }
      pMPEG4->save(inFile, outFile, 0);

      return true;
    }

    void Utils::parse_metadata(string &strFile) {
      fstream inFile(strFile.c_str(), ios::in | ios::binary | ios::ate);
      if (!inFile.is_open()) {
        cerr << "Error \"" << strFile << "\" does not exist or do not have permission." << endl;
        return;
      }
      inFile.close();
      int iArraySize = sizeof(MPEG_FILE_EXTENSIONS) / sizeof(MPEG_FILE_EXTENSIONS[1]);
      string strExt;
      string::size_type idx = strFile.rfind('.');
      if (idx != string::npos)
        strExt = strFile.substr(idx + 1);

      cout << "Processing: " << strFile << endl;
      if (!inArray((char *)strExt.c_str(), MPEG_FILE_EXTENSIONS, iArraySize)) {
        cerr << "Unknown file type" << endl;
        return;
      }
      return parse_mpeg4(strFile);
    }

    bool Utils::inject_metadata(const string &strInFile, string &strOutFile, Metadata *pMetadata) {
      if (strInFile == strOutFile) {
        cerr << "Input and output cannot be the same" << endl;
        return false;
      }
      fstream inFile(strInFile.c_str(), ios::in | ios::binary | ios::ate);
      if (!inFile.is_open()) {
        cerr << "Error \"" << strInFile << "\" does not exist or do not have permission." << endl;
        return false;
      }
      inFile.close();
      int iArraySize = sizeof(MPEG_FILE_EXTENSIONS) / sizeof(MPEG_FILE_EXTENSIONS[1]);
      string strExt;
      string::size_type idx = strInFile.rfind('.');
      if (idx != string::npos)
        strExt = strInFile.substr(idx);

      cout << "Processing: " << strInFile << endl;
      if (!inArray((char *)strExt.c_str(), MPEG_FILE_EXTENSIONS, iArraySize)) {
        cerr << "Unknown file type" << endl;
        return false;
      }
      return inject_mpeg4(strInFile, strOutFile, pMetadata);
    }

    string &Utils::generate_spherical_xml(
      Projection projection,
      StereoMode stereo,
      string stitchingSoftware,
      int *crop
    ) {
      // Configure inject xml
      static string empty;

      int iSize = (int)(SPHERICAL_XML_CONTENTS.length() + stitchingSoftware.length());
      char *buffer = new char[iSize];
      sprintf_s(buffer, iSize, SPHERICAL_XML_CONTENTS.c_str(), stitchingSoftware.c_str());
      string spherical_xml(buffer);
      delete buffer;

      if (projection == Projection::EQUIRECT)
        spherical_xml += SPHERICAL_XML_CONTENTS_EQUIRECT;
      if (projection == Projection::CUBEMAP)
        spherical_xml += SPHERICAL_XML_CONTENTS_CUBEMAP;
      if (projection == Projection::SINGLE_FISHEYE)
        spherical_xml += SPHERICAL_XML_CONTENTS_SINGLE_FISHEYE;

      string additional_xml;
      if (stereo == StereoMode::SM_TOP_BOTTOM)
        additional_xml += SPHERICAL_XML_CONTENTS_TOP_BOTTOM;
      if (stereo == StereoMode::SM_LEFT_RIGHT)
        additional_xml += SPHERICAL_XML_CONTENTS_LEFT_RIGHT;

      if (crop) {
        //  -c CROP, --crop CROP  crop region. Must specify 6 integers in the form of
        //                        "w:h:f_w:f_h:x:y" where w=CroppedAreaImageWidthPixels
        //                        h=CroppedAreaImageHeightPixels f_w=FullPanoWidthPixels
        //                        f_h=FullPanoHeightPixels x=CroppedAreaLeftPixels
        //                        y=CroppedAreaTopPixels
        int cropped_width_pixels = crop[0];
        int cropped_height_pixels = crop[1];
        int full_width_pixels = crop[2];
        int full_height_pixels = crop[3];
        int cropped_offset_left_pixels = crop[4];
        int cropped_offset_top_pixels = crop[5];
        // This should never happen based on the crop regex.
        if (full_width_pixels <= 0 || full_height_pixels <= 0) {
          cerr << "Error with crop params: full pano dimensions are ";
          cerr << "invalid: width = " << full_width_pixels << " height = " << full_height_pixels;
          return empty;
        }
        if (cropped_width_pixels <= 0 || cropped_height_pixels <= 0 ||
          cropped_width_pixels > full_width_pixels || cropped_height_pixels > full_height_pixels) {
          cerr << "Error with crop params: cropped area dimensions are ";
          cerr << "invalid: width = " << cropped_width_pixels << " height = " << cropped_height_pixels;
          return empty;
        }
        // We are pretty restrictive and don't allow anything strange. There
        // could be use-cases for a horizontal offset that essentially
        // translates the domain, but we don't support this (so that no
        // extra work has to be done on the client).
        int total_width = cropped_offset_left_pixels + cropped_width_pixels;
        int total_height = cropped_offset_top_pixels + cropped_height_pixels;
        if (cropped_offset_left_pixels  < 0 || cropped_offset_top_pixels < 0 ||
          total_width > full_width_pixels || total_height > full_height_pixels) {
          cerr << "Error with crop params: cropped area offsets are ";
          cerr << "invalid: left = " << cropped_offset_left_pixels << " top = " << cropped_offset_top_pixels;
          cerr << " left+cropped width: " << total_width << "top+cropped height: " << total_height;
          return empty;
        }
        // OMG: printf need to fall back to ugly-ass c-style
        int iSize = (int)(SPHERICAL_XML_CONTENTS_CROP_FORMAT.length() + 6 * 5); // 6 variables up to 99999
        char *buffer = new char[iSize];
        snprintf(buffer, iSize, SPHERICAL_XML_CONTENTS_CROP_FORMAT.c_str(), cropped_width_pixels, cropped_height_pixels,
          full_width_pixels, full_height_pixels, cropped_offset_left_pixels, cropped_offset_top_pixels);
        additional_xml += buffer;
        delete buffer;
      }
      m_strSphericalXML = SPHERICAL_XML_HEADER + spherical_xml + additional_xml + SPHERICAL_XML_FOOTER;
      return m_strSphericalXML;
    }

    uint8_t Utils::get_descriptor_length(fstream &inFile) {
      // Derives the length of the MP4 elementary stream descriptor at the
      // current position in the input file.
      int t;
      uint8_t iVal = 0;
      uint8_t descriptor_length = 0;
      for (t = 0; t < 4; t++) {
        iVal = mpeg::Box::readUint8(inFile);
        // Python :: descriptor_length = (descriptor_length << 7 | ord(size_byte) & int("0x7f", 0))
        descriptor_length = (descriptor_length << 7) | (iVal & 0x7F);
        if (iVal != 0x80)
          break;
      }
      return descriptor_length;
    }

    int32_t Utils::get_expected_num_audio_components(string &strAmbisonicType, uint32_t iAmbisonicsOrder) {
      // Returns the expected number of ambisonic components for a given
      // ambisonic type and ambisonic order.
      if (strAmbisonicType == "periphonic")
        return ((iAmbisonicsOrder + 1) * (iAmbisonicsOrder + 1));
      return -1;
    }

    int32_t Utils::get_num_audio_channels(mpeg::Container *pSTSD, fstream &inFile) {
      if (!pSTSD)
        return -1;
      if (memcmp(pSTSD->m_name, mpeg::constants::TAG_STSD, 4) != 0) {
        cerr << "get_num_audio_channels should be given a STSD box" << endl;
        return -1;
      }

      int iArraySize = (int)(sizeof(mpeg::constants::SOUND_SAMPLE_DESCRIPTIONS) / sizeof(mpeg::constants::SOUND_SAMPLE_DESCRIPTIONS[0]));
      vector<mpeg::Box *>::iterator it = pSTSD->m_listContents.begin();
      while (it != pSTSD->m_listContents.end()) {
        mpeg::Container *pSample = (mpeg::Container *)*it++;
        if (memcmp(pSample->m_name, mpeg::constants::TAG_MP4A, 4) == 0)
          return get_aac_num_channels(pSample, inFile);
        else if (inArray(pSample->m_name, mpeg::constants::SOUND_SAMPLE_DESCRIPTIONS, iArraySize))
          return get_sample_description_num_channels(pSample, inFile);
      }
      return -1;
    }

    uint32_t Utils::get_sample_description_num_channels(mpeg::Container *pSample, fstream &inFile) {
      // Reads the number of audio channels from a sound sample description.
      int iPos = (int)inFile.tellg();
      inFile.seekg(pSample->content_start() + 8);

      int16_t iAudioChannels, iSampleSizeBytes;
      int16_t iVersion = mpeg::Box::readInt16(inFile);
      int16_t iRevLevel = mpeg::Box::readInt16(inFile);
      int32_t iVendor = mpeg::Box::readInt32(inFile);
      if (iVersion == 0) {
        iAudioChannels = mpeg::Box::readInt16(inFile);
        iSampleSizeBytes = mpeg::Box::readInt16(inFile);
      } else if (iVersion == 1) {
        iAudioChannels = mpeg::Box::readInt16(inFile);
        iSampleSizeBytes = mpeg::Box::readInt16(inFile);
        int32_t iBytesPerPacket, iBytesPerFrame, iBytesPerSample;
        iBytesPerPacket = mpeg::Box::readInt32(inFile);
        iBytesPerFrame = mpeg::Box::readInt32(inFile);
        iBytesPerSample = mpeg::Box::readInt32(inFile);
      } else if (iVersion == 2) {
        int16_t always_3, always_16, always_minus_2, always_0;
        int32_t always_65536, size_of_struct_only;
        double  audio_sample_rate;
        always_3 = mpeg::Box::readInt16(inFile);
        always_16 = mpeg::Box::readInt16(inFile);
        always_minus_2 = mpeg::Box::readInt16(inFile);
        always_0 = mpeg::Box::readInt16(inFile);
        always_65536 = mpeg::Box::readInt32(inFile);
        size_of_struct_only = mpeg::Box::readInt32(inFile);
        audio_sample_rate = mpeg::Box::readDouble(inFile);
        iAudioChannels = mpeg::Box::readInt16(inFile);
      } else {
        inFile.seekg(iPos);
        cerr << "Unsupported version for " << pSample->name() << " box" << endl;
        return -1;
      }
      inFile.seekg(iPos);
      return iAudioChannels;
    }

    int32_t Utils::get_aac_num_channels(mpeg::Container *pBox, fstream &inFile) {
      // Reads the number of audio channels from AAC's AudioSpecificConfig
      // descriptor within the esds child box of the input mp4a or wave box.
      int iPos = (int)inFile.tellg();
      bool bFound = memcmp(pBox->m_name, mpeg::constants::TAG_MP4A, 4) == 0;
      bFound |= memcmp(pBox->m_name, mpeg::constants::TAG_WAVE, 4) == 0;
      if (!bFound)
        return -1;

      int32_t channel_configuration = 0;
      vector<mpeg::Box *>::iterator it = pBox->m_listContents.begin();
      while (it != pBox->m_listContents.end()) {
        mpeg::Container *pElement = (mpeg::Container *)*it++;
        if (memcmp(pElement->m_name, mpeg::constants::TAG_WAVE, 4) == 0) {
          channel_configuration = get_aac_num_channels(pElement, inFile);
          break;
        }
        if (memcmp(pElement->m_name, mpeg::constants::TAG_ESDS, 4) != 0)
          continue;

        inFile.seekg(pElement->content_start() + 4);
        char descriptor_tag = mpeg::Box::readInt8(inFile);

        // Verify the read descriptor is an elementary stream descriptor
        if (descriptor_tag != 3) { // Not an MP4 elementary stream.
          cerr << "Error: failed to read elementary stream descriptor." << endl;
          return -1;
        }
        get_descriptor_length(inFile);
        inFile.seekg(3, ios_base::cur); // Seek to the decoder configuration descriptor
        char config_descriptor_tag = mpeg::Box::readInt8(inFile);

        // Verify the read descriptor is a decoder config. descriptor.
        if (config_descriptor_tag != 4) {
          cerr << "Error: failed to read decoder config. descriptor." << endl;
          return -1;
        }
        get_descriptor_length(inFile);
        inFile.seekg(13, ios_base::cur); // offset to the decoder specific config descriptor.
        char decoder_specific_descriptor_tag = mpeg::Box::readInt8(inFile);

        // Verify the read descriptor is a decoder specific info descriptor
        if (decoder_specific_descriptor_tag != 5) {
          cerr << "Error: failed to read MP4 audio decoder specific config." << endl;
          return -1;
        }
        int audio_specific_descriptor_size = get_descriptor_length(inFile);
        // assert ( audio_specific_descriptor_size >= 2 );
        int16_t  decoder_descriptor = mpeg::Box::readInt16(inFile);
        uint32_t object_type = (0xF800 & decoder_descriptor) >> 11;
        uint32_t sampling_frequency_index = (0x0780 & decoder_descriptor) >> 7;
        if (sampling_frequency_index == 0) {
          // TODO: If the sample rate is 96kHz an additional 24 bit offset
          // value here specifies the actual sample rate.
          cerr << "Error: Greater than 48khz audio is currently not supported." << endl;
          return -1;
        }
        channel_configuration = (0x0078 & decoder_descriptor) >> 3;
      }
      inFile.seekg(iPos);
      return channel_configuration;
    }

    uint32_t Utils::get_num_audio_tracks(mpeg::Mpeg4Container *pMPEG4, fstream &inFile) {
      // Returns the number of audio track in the input mpeg4 file.
      int num_audio_tracks = 0;
      mpeg::Container *pMoov = (mpeg::Container *)pMPEG4->m_pMoovBox;
      if (!pMoov)
        return 0;

      vector<mpeg::Box *>::iterator it = pMoov->m_listContents.begin();
      while (it != pMoov->m_listContents.end()) {
        mpeg::Container *pBox = (mpeg::Container *)*it++;
        if (memcmp(pBox->m_name, mpeg::constants::TAG_TRAK, 4) != 0)
          continue;

        vector<mpeg::Box *>::iterator it2 = pBox->m_listContents.begin();
        while (it2 != pBox->m_listContents.end()) {
          mpeg::Container *pSub = (mpeg::Container *)*it2++;
          if (memcmp(pSub->m_name, mpeg::constants::TAG_MDIA, 4) != 0)
            continue;

          vector<mpeg::Box *>::iterator it3 = pSub->m_listContents.begin();
          while (it3 != pSub->m_listContents.end()) {
            mpeg::Container *pMDIA = (mpeg::Container *)*it3++;
            if (memcmp(pMDIA->m_name, mpeg::constants::TAG_HDLR, 4) != 0)
              continue;

            int iPos = pMDIA->content_start() + 8;
            inFile.seekg(iPos);
            char buffer[4];
            inFile.read(buffer, 4);
            if (memcmp(buffer, mpeg::constants::TAG_SOUN, 4) == 0)
              num_audio_tracks++;
          }
        }
      }

      return num_audio_tracks;
    }
  }
}