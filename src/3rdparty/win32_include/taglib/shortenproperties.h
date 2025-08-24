/***************************************************************************
    copyright           : (C) 2020-2024 Stephen F. Booth
    email               : me@sbooth.org
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

#ifndef TAGLIB_SHORTENPROPERTIES_H
#define TAGLIB_SHORTENPROPERTIES_H

#include <memory>

#include "taglib_export.h"
#include "audioproperties.h"

namespace TagLib {
  namespace Shorten {

    struct PropertyValues;

    //! An implementation of audio properties for Shorten
    class TAGLIB_EXPORT Properties : public AudioProperties {
    public:
      Properties(const PropertyValues *values, ReadStyle style = Average);
      ~Properties() override;

      Properties(const Properties &) = delete;
      Properties &operator=(const Properties &) = delete;

      int lengthInMilliseconds() const override;
      int bitrate() const override;
      int sampleRate() const override;
      int channels() const override;

      //! Returns the Shorten file version (1-3).
      int shortenVersion() const;
      //! Returns the file type (0-9).
      //! 0 = 8-bit µ-law,
      //! 1 = signed 8-bit PCM, 2 = unsigned 8-bit PCM,
      //! 3 = signed big-endian 16-bit PCM, 4 = unsigned big-endian 16-bit PCM,
      //! 5 = signed little-endian 16-bit PCM, 6 = unsigned little-endian 16-bit PCM,
      //! 7 = 8-bit ITU-T G.711 µ-law, 8 = 8-bit µ-law,
      //! 9 = 8-bit A-law, 10 = 8-bit ITU-T G.711 A-law
      int fileType() const;
      int bitsPerSample() const;
      unsigned long sampleFrames() const;

    private:
      class PropertiesPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<PropertiesPrivate> d;
    };
  }  // namespace Shorten
}  // namespace TagLib

#endif
