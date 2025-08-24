/***************************************************************************
    copyright            : (C) 2006 by Lukáš Lalinský
    email                : lalinsky@gmail.com

    copyright            : (C) 2004 by Allan Sandfeld Jensen
    email                : kde@carewolf.org
                           (original MPC implementation)
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

#ifndef TAGLIB_WVPROPERTIES_H
#define TAGLIB_WVPROPERTIES_H

#include "taglib_export.h"
#include "audioproperties.h"

namespace TagLib {

  namespace WavPack {

    class File;

    static constexpr unsigned int HeaderSize = 32;

    //! An implementation of audio property reading for WavPack

    /*!
     * This reads the data from a WavPack stream found in the AudioProperties
     * API.
     */

    class TAGLIB_EXPORT Properties : public AudioProperties
    {
    public:
      /*!
       * Create an instance of WavPack::Properties.
       */
      Properties(File *file, offset_t streamLength, ReadStyle style = Average);

      /*!
       * Destroys this WavPack::Properties instance.
       */
      ~Properties() override;

      Properties(const Properties &) = delete;
      Properties &operator=(const Properties &) = delete;

      /*!
       * Returns the length of the file in milliseconds.
       *
       * \see lengthInSeconds()
       */
      int lengthInMilliseconds() const override;

      /*!
       * Returns the average bit rate of the file in kb/s.
       */
      int bitrate() const override;

      /*!
       * Returns the sample rate in Hz. 0 means unknown or custom.
       */
      int sampleRate() const override;

      /*!
       * Returns the number of audio channels.
       */
      int channels() const override;

      /*!
       * Returns the number of bits per audio sample.
       */
      int bitsPerSample() const;

      /*!
       * Returns whether or not the file is lossless encoded.
       */
      bool isLossless() const;

      /*!
       * Returns the total number of audio samples in file.
       */
      unsigned int sampleFrames() const;

      /*!
       * Returns the WavPack version.
       */
      int version() const;

    private:
      void read(File *file, offset_t streamLength);
      unsigned int seekFinalIndex(File *file, offset_t streamLength);

      class PropertiesPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<PropertiesPrivate> d;
    };
  }  // namespace WavPack
}  // namespace TagLib

#endif
