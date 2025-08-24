/***************************************************************************
    copyright           : (C) 2013-2023 Stephen F. Booth
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

#ifndef TAGLIB_DSFPROPERTIES_H
#define TAGLIB_DSFPROPERTIES_H

#include <memory>

#include "taglib_export.h"
#include "tbytevector.h"
#include "audioproperties.h"

namespace TagLib {
  namespace DSF {
    //! An implementation of audio properties for DSF
    class TAGLIB_EXPORT Properties : public AudioProperties {
    public:
      Properties(const ByteVector &data, ReadStyle style);
      ~Properties() override;

      Properties(const Properties &) = delete;
      Properties &operator=(const Properties &) = delete;

      int lengthInMilliseconds() const override;
      int bitrate() const override;
      int sampleRate() const override;
      int channels() const override;

      int formatVersion() const;
      int formatID() const;

      /*!
       * Channel type values: 1 = mono, 2 = stereo, 3 = 3 channels,
       * 4 = quad, 5 = 4 channels, 6 = 5 channels, 7 = 5.1 channels
       */
      int channelType() const;
      int bitsPerSample() const;
      long long sampleCount() const;
      int blockSizePerChannel() const;

    private:
      void read(const ByteVector &data);

      class PropertiesPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<PropertiesPrivate> d;
    };
  }  // namespace DSF
}  // namespace TagLib

#endif
