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

#ifndef TAGLIB_XMPROPERTIES_H
#define TAGLIB_XMPROPERTIES_H

#include "tstring.h"
#include "audioproperties.h"

namespace TagLib {
  namespace XM {
    //! An implementation of audio property reading for XM
    class TAGLIB_EXPORT Properties : public AudioProperties {
    public:
      /*! Flag bits. */
      enum {
        LinearFreqTable = 1 // otherwise it is the amiga freq. table
      };

      Properties(AudioProperties::ReadStyle propertiesStyle);
      ~Properties() override;

      Properties(const Properties &) = delete;
      Properties &operator=(const Properties &) = delete;

      int channels() const override;

      unsigned short lengthInPatterns() const;
      unsigned short version() const;
      unsigned short restartPosition() const;
      unsigned short patternCount() const;
      unsigned short instrumentCount() const;
      unsigned int sampleCount() const;
      unsigned short flags() const;
      unsigned short tempo() const;
      unsigned short bpmSpeed() const;

      void setChannels(int channels);

      void setLengthInPatterns(unsigned short lengthInPatterns);
      void setVersion(unsigned short version);
      void setRestartPosition(unsigned short restartPosition);
      void setPatternCount(unsigned short patternCount);
      void setInstrumentCount(unsigned short instrumentCount);
      void setSampleCount(unsigned int sampleCount);
      void setFlags(unsigned short flags);
      void setTempo(unsigned short tempo);
      void setBpmSpeed(unsigned short bpmSpeed);

    private:
      class PropertiesPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<PropertiesPrivate> d;
    };
  }  // namespace XM
}  // namespace TagLib

#endif
