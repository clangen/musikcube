/***************************************************************************
    copyright            : (C) 2023 by Urs Fleisch
    email                : ufleisch@users.sourceforge.net
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

#ifndef TAGLIB_PICTURETYPE_H
#define TAGLIB_PICTURETYPE_H

// THIS FILE IS NOT A PART OF THE TAGLIB API

#ifndef DO_NOT_DOCUMENT  // tell Doxygen not to document this header

#include "taglib_export.h"

/*!
 * Declare a picture type \a name enumeration inside a class.
 * Declares a picture type enum according to the ID3v2 specification and
 * adds methods \c typeToString() and \c typeFromString().
 *
 * \code {.cpp}
 * class MyClass {
 * public:
 *   DECLARE_PICTURE_TYPE_ENUM(Type)
 *   (..)
 * }
 * \endcode
 */
#define DECLARE_PICTURE_TYPE_ENUM(name)                       \
enum name {                                                   \
  /*! A type not enumerated below */                          \
  Other              = 0x00,                                  \
  /*! 32x32 PNG image that should be used as the file icon */ \
  FileIcon           = 0x01,                                  \
  /*! File icon of a different size or format */              \
  OtherFileIcon      = 0x02,                                  \
  /*! Front cover image of the album */                       \
  FrontCover         = 0x03,                                  \
  /*! Back cover image of the album */                        \
  BackCover          = 0x04,                                  \
  /*! Inside leaflet page of the album */                     \
  LeafletPage        = 0x05,                                  \
  /*! Image from the album itself */                          \
  Media              = 0x06,                                  \
  /*! Picture of the lead artist or soloist */                \
  LeadArtist         = 0x07,                                  \
  /*! Picture of the artist or performer */                   \
  Artist             = 0x08,                                  \
  /*! Picture of the conductor */                             \
  Conductor          = 0x09,                                  \
  /*! Picture of the band or orchestra */                     \
  Band               = 0x0A,                                  \
  /*! Picture of the composer */                              \
  Composer           = 0x0B,                                  \
  /*! Picture of the lyricist or text writer */               \
  Lyricist           = 0x0C,                                  \
  /*! Picture of the recording location or studio */          \
  RecordingLocation  = 0x0D,                                  \
  /*! Picture of the artists during recording */              \
  DuringRecording    = 0x0E,                                  \
  /*! Picture of the artists during performance */            \
  DuringPerformance  = 0x0F,                                  \
  /*! Picture from a movie or video related to the track */   \
  MovieScreenCapture = 0x10,                                  \
  /*! Picture of a large, coloured fish */                    \
  ColouredFish       = 0x11,                                  \
  /*! Illustration related to the track */                    \
  Illustration       = 0x12,                                  \
  /*! Logo of the band or performer */                        \
  BandLogo           = 0x13,                                  \
  /*! Logo of the publisher (record company) */               \
  PublisherLogo      = 0x14                                   \
};                                                            \
static TagLib::String typeToString(name type) {               \
  return TagLib::Utils::pictureTypeToString(type);            \
}                                                             \
static name typeFromString(const TagLib::String &str) {       \
  return static_cast<name>(                                   \
    TagLib::Utils::pictureTypeFromString(str));               \
}

namespace TagLib {

  class String;

  namespace Utils {

    /*!
     * Get string representation of picture type.
     */
    String TAGLIB_EXPORT pictureTypeToString(int type);

    /*!
     * Get picture type from string representation.
     */
    int TAGLIB_EXPORT pictureTypeFromString(const String& str);

  }  // namespace Utils
}  // namespace TagLib

#endif

#endif
