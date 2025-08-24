/***************************************************************************
    copyright            : (C) 2003 by Ismael Orenstein
    email                : orenstein@kde.org
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

#ifndef TAGLIB_XINGHEADER_H
#define TAGLIB_XINGHEADER_H

#include <memory>

#include "taglib_export.h"
#include "mpegheader.h"

namespace TagLib {

  class ByteVector;

  namespace MPEG {

    //! An implementation of the Xing/VBRI headers

    /*!
     * This is a minimalistic implementation of the Xing/VBRI VBR headers.
     * Xing/VBRI headers are often added to VBR (variable bit rate) MP3 streams
     * to make it easy to compute the length and quality of a VBR stream.  Our
     * implementation is only concerned with the total size of the stream (so
     * that we can calculate the total playing time and the average bitrate).
     * It uses <a href="https://multimedia.cx/mp3extensions.txt">
     * mp3extensions.txt</a> and the XMMS sources as references.
     */

    class TAGLIB_EXPORT XingHeader
    {
    public:
      /*!
       * The type of the VBR header.
       */
      enum HeaderType
      {
        /*!
         * Invalid header or no VBR header found.
         */
        Invalid = 0,

        /*!
         * Xing header.
         */
        Xing = 1,

        /*!
         * VBRI header.
         */
        VBRI = 2,
      };

      /*!
       * Parses a Xing/VBRI header based on \a data which contains the entire
       * first MPEG frame.
       */
      XingHeader(const ByteVector &data);

      /*!
       * Destroy this XingHeader instance.
       */
      ~XingHeader();

      XingHeader(const XingHeader &) = delete;
      XingHeader &operator=(const XingHeader &) = delete;

      /*!
       * Returns \c true if the data was parsed properly and if there is a valid
       * Xing/VBRI header present.
       */
      bool isValid() const;

      /*!
       * Returns the total number of frames.
       */
      unsigned int totalFrames() const;

      /*!
       * Returns the total size of stream in bytes.
       */
      unsigned int totalSize() const;

      /*!
       * Returns the type of the VBR header.
       */
      HeaderType type() const;

    private:
      void parse(const ByteVector &data);

      class XingHeaderPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<XingHeaderPrivate> d;
    };
  }  // namespace MPEG
}  // namespace TagLib

#endif
