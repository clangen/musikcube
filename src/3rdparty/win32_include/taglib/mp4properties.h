/**************************************************************************
    copyright            : (C) 2007 by Lukáš Lalinský
    email                : lalinsky@gmail.com
 **************************************************************************/

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

#ifndef TAGLIB_MP4PROPERTIES_H
#define TAGLIB_MP4PROPERTIES_H

#include "taglib_export.h"
#include "audioproperties.h"

namespace TagLib {
  namespace MP4 {
    class Atoms;
    class File;

    //! An implementation of MP4 audio properties
    class TAGLIB_EXPORT Properties : public AudioProperties
    {
    public:
      enum Codec {
        Unknown = 0,
        AAC,
        ALAC
      };

      Properties(File *file, const Atoms *atoms, ReadStyle style = Average);
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
      virtual int bitsPerSample() const;

      /*!
       * Returns whether or not the file is encrypted.
       */
      bool isEncrypted() const;

      /*!
       * Returns the codec used in the file.
       */
      Codec codec() const;

    private:
      void read(File *file, const Atoms *atoms);

      class PropertiesPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<PropertiesPrivate> d;
    };
  }  // namespace MP4
}  // namespace TagLib
#endif
