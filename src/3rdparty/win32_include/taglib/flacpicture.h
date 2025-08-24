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

#ifndef TAGLIB_FLACPICTURE_H
#define TAGLIB_FLACPICTURE_H

#include "tlist.h"
#include "tstring.h"
#include "tbytevector.h"
#include "tpicturetype.h"
#include "taglib_export.h"
#include "flacmetadatablock.h"

namespace TagLib {
  namespace FLAC {
    //! FLAC picture
    class TAGLIB_EXPORT Picture : public MetadataBlock
    {
    public:

      /*
       * This describes the function or content of the picture.
       */
      DECLARE_PICTURE_TYPE_ENUM(Type)

      Picture();
      Picture(const ByteVector &data);
      ~Picture() override;

      Picture(const Picture &item) = delete;
      Picture &operator=(const Picture &item) = delete;

      /*!
       * Returns the type of the image.
       */
      Type type() const;

      /*!
       * Sets the type of the image.
       */
      void setType(Type type);

      /*!
       * Returns the mime type of the image.  This should in most cases be
       * "image/png" or "image/jpeg".
       */
      String mimeType() const;

      /*!
       * Sets the mime type of the image.  This should in most cases be
       * "image/png" or "image/jpeg".
       */
      void setMimeType(const String &mimeType);

      /*!
       * Returns a text description of the image.
       */

      String description() const;

      /*!
       * Sets a textual description of the image to \a description.
       */

      void setDescription(const String &description);

      /*!
       * Returns the width of the image.
       */
      int width() const;

      /*!
       * Sets the width of the image.
       */
      void setWidth(int width);

      /*!
       * Returns the height of the image.
       */
      int height() const;

      /*!
       * Sets the height of the image.
       */
      void setHeight(int height);

      /*!
       * Returns the color depth (in bits-per-pixel) of the image.
       */
      int colorDepth() const;

      /*!
       * Sets the color depth (in bits-per-pixel) of the image.
       */
      void setColorDepth(int colorDepth);

      /*!
       * Returns the number of colors used on the image..
       */
      int numColors() const;

      /*!
       * Sets the number of colors used on the image (for indexed images).
       */
      void setNumColors(int numColors);

      /*!
       * Returns the image data.
       */
      ByteVector data() const;

      /*!
       * Sets the image data.
       */
      void setData(const ByteVector &data);

      /*!
       * Returns the FLAC metadata block type.
       */
      int code() const override;

      /*!
       * Render the content to the FLAC picture block format.
       */
      ByteVector render() const override;

      /*!
       * Parse the picture data in the FLAC picture block format.
       */
      bool parse(const ByteVector &data);

    private:
      class PicturePrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<PicturePrivate> d;
    };

    using PictureList = List<Picture>;
  }  // namespace FLAC
}  // namespace TagLib
#endif
