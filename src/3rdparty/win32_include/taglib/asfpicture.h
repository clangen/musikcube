/**************************************************************************
    copyright            : (C) 2010 by Anton Sergunov
    email                : setosha@gmail.com
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

#ifndef ASFPICTURE_H
#define ASFPICTURE_H

#include "tstring.h"
#include "tbytevector.h"
#include "tpicturetype.h"
#include "taglib_export.h"

namespace TagLib
{
  namespace ASF
  {

    //! An ASF attached picture interface implementation

    /*!
     * This is an implementation of ASF attached pictures.  Pictures may be
     * included in attributes, one per WM/Picture attribute (but there may be multiple WM/Picture
     * attributes in a single tag).  These pictures are usually in either JPEG or
     * PNG format.
     * \see Attribute::toPicture()
     * \see Attribute::Attribute(const Picture& picture)
     */
    class TAGLIB_EXPORT Picture {
    public:

      /*
       * This describes the function or content of the picture.
       */
      DECLARE_PICTURE_TYPE_ENUM(Type)

      /*!
       * Constructs an empty picture.
       */
      Picture();

      /*!
       * Construct a picture as a copy of \a other.
       */
      Picture(const Picture& other);

      /*!
       * Destroys the picture.
       */
      ~Picture();

      /*!
       * Copies the contents of \a other into this picture.
       */
      Picture& operator=(const Picture& other);

      /*!
       * Exchanges the content of the Picture with the content of \a other.
       */
      void swap(Picture &other) noexcept;

      /*!
       * Returns \c true if Picture stores valid picture
       */
      bool isValid() const;

      /*!
       * Returns the mime type of the image. This should in most cases be
       * "image/png" or "image/jpeg".
       * \see setMimeType(const String &)
       * \see picture()
       * \see setPicture(const ByteArray&)
       */
      String mimeType() const;

      /*!
       * Sets the mime type of the image.  This should in most cases be
       * "image/png" or "image/jpeg".
       * \see setMimeType(const String &)
       * \see picture()
       * \see setPicture(const ByteArray&)
       */
      void setMimeType(const String &value);

      /*!
       * Returns the type of the image.
       *
       * \see Type
       * \see setType()
       */
      Type type() const;

      /*!
       * Sets the type for the image.
       *
       * \see Type
       * \see type()
       */
      void setType(const ASF::Picture::Type& t);

      /*!
       * Returns a text description of the image.
       *
       * \see setDescription()
       */
      String description() const;

      /*!
       * Sets a textual description of the image to \a desc.
       *
       * \see description()
       */
      void setDescription(const String &desc);

      /*!
       * Returns the image data as a ByteVector.
       *
       * \note ByteVector has a data() method that returns a <tt>const char *</tt> which
       * should make it easy to export this data to external programs.
       *
       * \see setPicture()
       * \see mimeType()
       */
      ByteVector picture() const;

      /*!
       * Sets the image data to \a p.  \a p should be of the type specified in
       * this frame's mime-type specification.
       *
       * \see picture()
       * \see mimeType()
       * \see setMimeType()
       */
      void setPicture(const ByteVector &p);

      /*!
       * Returns picture as binary raw data \a value
       */
      ByteVector render() const;

      /*!
       * Returns picture as binary raw data \a value
       */
      int dataSize() const;

#ifndef DO_NOT_DOCUMENT
      /* THIS IS PRIVATE, DON'T TOUCH IT! */
      void parse(const ByteVector& );
      static Picture fromInvalid();
#endif

      private:
        class PicturePrivate;
        TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
        std::shared_ptr<PicturePrivate> d;
      };
  }  // namespace ASF
}  // namespace TagLib

#endif // ASFPICTURE_H
