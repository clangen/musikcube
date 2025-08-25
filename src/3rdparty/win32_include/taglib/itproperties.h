/***************************************************************************
    copyright           : (C) 2011 by Mathias Panzenböck
    email               : grosser.meister.morti@gmx.net
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License version   *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA         *
 *   02110-1301  USA                                                       *
 *                                                                         *
 *   Alternatively, this file is available under the Mozilla Public        *
 *   License Version 1.1.  You may obtain a copy of the License at         *
 *   http://www.mozilla.org/MPL/                                           *
 ***************************************************************************/

#ifndef TAGLIB_ITPROPERTIES_H
#define TAGLIB_ITPROPERTIES_H

#include "audioproperties.h"

namespace TagLib {
  namespace IT {
    //! An implementation of audio property reading for IT
    class TAGLIB_EXPORT Properties : public AudioProperties {
    public:
      /*! Flag bits. */
      enum {
        Stereo                  =   1,
        Vol0MixOptimizations    =   2,
        UseInstruments          =   4,
        LinearSlides            =   8,
        OldEffects              =  16,
        LinkEffects             =  32,
        UseMidiPitchController  =  64,
        RequestEmbeddedMidiConf = 128
      };

      /*! Special bits. */
      enum {
        MessageAttached  = 1,
        MidiConfEmbedded = 8
      };

      Properties(AudioProperties::ReadStyle propertiesStyle);
      ~Properties() override;

      Properties(const Properties &) = delete;
      Properties &operator=(const Properties &) = delete;

      int channels() const override;

      unsigned short lengthInPatterns() const;
      bool stereo() const;
      unsigned short instrumentCount() const;
      unsigned short sampleCount() const;
      unsigned short patternCount() const;
      unsigned short version() const;
      unsigned short compatibleVersion() const;
      unsigned short flags() const;
      unsigned short special() const;
      unsigned char globalVolume() const;
      unsigned char mixVolume() const;
      unsigned char tempo() const;
      unsigned char bpmSpeed() const;
      unsigned char panningSeparation() const;
      unsigned char pitchWheelDepth() const;

      void setChannels(int channels);
      void setLengthInPatterns(unsigned short lengthInPatterns);
      void setInstrumentCount(unsigned short instrumentCount);
      void setSampleCount(unsigned short sampleCount);
      void setPatternCount(unsigned short patternCount);
      void setVersion(unsigned short version);
      void setCompatibleVersion(unsigned short compatibleVersion);
      void setFlags(unsigned short flags);
      void setSpecial(unsigned short special);
      void setGlobalVolume(unsigned char globalVolume);
      void setMixVolume(unsigned char mixVolume);
      void setTempo(unsigned char tempo);
      void setBpmSpeed(unsigned char bpmSpeed);
      void setPanningSeparation(unsigned char panningSeparation);
      void setPitchWheelDepth(unsigned char pitchWheelDepth);

    private:
      class PropertiesPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<PropertiesPrivate> d;
    };
  }  // namespace IT
}  // namespace TagLib

#endif
