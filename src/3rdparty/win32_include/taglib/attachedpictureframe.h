/***************************************************************************
    copyright            : (C) 2002 - 2008 by Scott Wheeler
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

#ifndef TAGLIB_ATTACHEDPICTUREFRAME_H
#define TAGLIB_ATTACHEDPICTUREFRAME_H

#include "taglib_export.h"
#include "tpicturetype.h"
#include "id3v2frame.h"

namespace TagLib {

  namespace ID3v2 {

    //! An ID3v2 attached picture frame implementation

    /*!
     * This is an implementation of ID3v2 attached pictures.  Pictures may be
     * included in tags, one per APIC frame (but there may be multiple APIC
     * frames in a single tag).  These pictures are usually in either JPEG or
     * PNG format.
     */

    class TAGLIB_EXPORT AttachedPictureFrame : public Frame
    {
      friend class FrameFactory;

    public:

      /*
       * This describes the function or content of the picture.
       */
      DECLARE_PICTURE_TYPE_ENUM(Type)

      /*!
       * Constructs an empty picture frame.  The description, content and text
       * encoding should be set manually.
       */
      AttachedPictureFrame();

      /*!
       * Constructs an AttachedPicture frame based on \a data.
       */
      explicit AttachedPictureFrame(const ByteVector &data);

      /*!
       * Destroys the AttachedPictureFrame instance.
       */
      ~AttachedPictureFrame() override;

      AttachedPictureFrame(const AttachedPictureFrame &) = delete;
      AttachedPictureFrame &operator=(const AttachedPictureFrame &) = delete;

      /*!
       * Returns a string containing the description and mime-type
       */
      String toString() const override;

      /*!
       * Returns a string list containing the description and mime-type.
       */
      StringList toStringList() const override;

      /*!
       * Returns the text encoding used for the description.
       *
       * \see setTextEncoding()
       * \see description()
       */
      String::Type textEncoding() const;

      /*!
       * Set the text encoding used for the description.
       *
       * \see description()
       */
      void setTextEncoding(String::Type t);

      /*!
       * Returns the mime type of the image.  This should in most cases be
       * "image/png" or "image/jpeg".
       */
      String mimeType() const;

      /*!
       * Sets the mime type of the image.  This should in most cases be
       * "image/png" or "image/jpeg".
       */
      void setMimeType(const String &m);

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
      void setType(Type t);

      /*!
       * Returns a text description of the image.
       *
       * \see setDescription()
       * \see textEncoding()
       * \see setTextEncoding()
       */

      String description() const;

      /*!
       * Sets a textual description of the image to \a desc.
       *
       * \see description()
       * \see textEncoding()
       * \see setTextEncoding()
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

    protected:
      void parseFields(const ByteVector &data) override;
      ByteVector renderFields() const override;
      class AttachedPictureFramePrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<AttachedPictureFramePrivate> d;

    private:
      AttachedPictureFrame(const ByteVector &data, Header *h);
    };

    //! support for ID3v2.2 PIC frames
    class TAGLIB_EXPORT AttachedPictureFrameV22 : public AttachedPictureFrame
    {
    protected:
      void parseFields(const ByteVector &data) override;
    private:
      AttachedPictureFrameV22(const ByteVector &data, Header *h);
      friend class FrameFactory;
    };
  }  // namespace ID3v2
}  // namespace TagLib

#endif
