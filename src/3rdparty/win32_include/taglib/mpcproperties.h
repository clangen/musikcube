/***************************************************************************
    copyright            : (C) 2004 by Allan Sandfeld Jensen
    email                : kde@carewolf.org
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

#ifndef TAGLIB_MPCPROPERTIES_H
#define TAGLIB_MPCPROPERTIES_H

#include "tbytevector.h"
#include "taglib_export.h"
#include "audioproperties.h"

namespace TagLib {

  namespace MPC {

    class File;

    static constexpr unsigned int HeaderSize = 8 * 7;

    //! An implementation of audio property reading for MPC

    /*!
     * This reads the data from an MPC stream found in the AudioProperties
     * API.
     */

    class TAGLIB_EXPORT Properties : public AudioProperties
    {
    public:
      /*!
       * Create an instance of MPC::Properties with the data read directly
       * from an MPC::File.
       */
      Properties(File *file, offset_t streamLength, ReadStyle style = Average);

      /*!
       * Destroys this MPC::Properties instance.
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
       * Returns the sample rate in Hz.
       */
      int sampleRate() const override;

      /*!
       * Returns the number of audio channels.
       */
      int channels() const override;

      /*!
       * Returns the version of the bitstream (SV4-SV8)
       */
      int mpcVersion() const;

      unsigned int totalFrames() const;
      unsigned long sampleFrames() const;

      /*!
      * Returns the track gain as an integer value,
      * to convert to dB: trackGain in dB = 64.82 - (trackGain / 256)
      */
      int trackGain() const;

      /*!
      * Returns the track peak as an integer value,
      * to convert to dB: trackPeak in dB = trackPeak / 256
      * to convert to floating [-1..1]: trackPeak = 10^(trackPeak / 256 / 20)/32768
      */
      int trackPeak() const;

      /*!
      * Returns the album gain as an integer value,
      * to convert to dB: albumGain in dB = 64.82 - (albumGain / 256)
      */
      int albumGain() const;

      /*!
      * Returns the album peak as an integer value,
      * to convert to dB: albumPeak in dB = albumPeak / 256
      * to convert to floating [-1..1]: albumPeak = 10^(albumPeak / 256 / 20)/32768
      */
      int albumPeak() const;

    private:
      void readSV7(const ByteVector &data, offset_t streamLength);
      void readSV8(File *file, offset_t streamLength);

      class PropertiesPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<PropertiesPrivate> d;
    };
  }  // namespace MPC
}  // namespace TagLib

#endif
