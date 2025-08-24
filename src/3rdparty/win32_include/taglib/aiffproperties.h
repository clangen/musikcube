/***************************************************************************
    copyright            : (C) 2008 by Scott Wheeler
    email                : wheeler@kde.org
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

#ifndef TAGLIB_AIFFPROPERTIES_H
#define TAGLIB_AIFFPROPERTIES_H

#include "tstring.h"
#include "audioproperties.h"

namespace TagLib {

  namespace RIFF {

    namespace AIFF {

      class File;

      //! An implementation of audio property reading for AIFF

      /*!
       * This reads the data from an AIFF stream found in the AudioProperties
       * API.
       */

      class TAGLIB_EXPORT Properties : public AudioProperties
      {
      public:
        /*!
         * Create an instance of AIFF::Properties with the data read from the
         * AIFF::File \a file.
         */
        Properties(File *file, ReadStyle style);

        /*!
         * Destroys this AIFF::Properties instance.
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
         * Returns the number of bits per audio sample.
         */
        int bitsPerSample() const;

        /*!
         * Returns the number of sample frames
         */
        unsigned int sampleFrames() const;

        /*!
         * Returns \c true if the file is in AIFF-C format, \c false if AIFF format.
         */
        bool isAiffC() const;

        /*!
         * Returns the compression type of the AIFF-C file.  For example, "NONE" for
         * not compressed, "ACE2" for ACE 2-to-1.
         *
         * If the file is in AIFF format, always returns an empty vector.
         *
         * \see isAiffC()
         */
        ByteVector compressionType() const;

        /*!
         * Returns the concrete compression name of the AIFF-C file.
         *
         * If the file is in AIFF format, always returns an empty string.
         *
         * \see isAiffC()
         */
        String compressionName() const;

      private:
        void read(File *file);

        class PropertiesPrivate;
        TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
        std::unique_ptr<PropertiesPrivate> d;
      };
    }  // namespace AIFF
  }  // namespace RIFF
}  // namespace TagLib

#endif
