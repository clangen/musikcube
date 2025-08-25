/**************************************************************************
    copyright            : (C) 2010 by Lukáš Lalinský
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

#ifndef TAGLIB_FLACMETADATABLOCK_H
#define TAGLIB_FLACMETADATABLOCK_H

#include "tbytevector.h"
#include "taglib_export.h"

namespace TagLib {
  namespace FLAC {
    //! FLAC metadata block
    class TAGLIB_EXPORT MetadataBlock
    {
    public:
      MetadataBlock();
      virtual ~MetadataBlock();

      MetadataBlock(const MetadataBlock &item) = delete;
      MetadataBlock &operator=(const MetadataBlock &item) = delete;

      enum BlockType {
        StreamInfo = 0,
        Padding,
        Application,
        SeekTable,
        VorbisComment,
        CueSheet,
        Picture
      };

      /*!
       * Returns the FLAC metadata block type.
       */
      virtual int code() const = 0;

      /*!
       * Render the content of the block.
       */
      virtual ByteVector render() const = 0;

    private:
      class MetadataBlockPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<MetadataBlockPrivate> d;
    };
  }  // namespace FLAC
}  // namespace TagLib
#endif
