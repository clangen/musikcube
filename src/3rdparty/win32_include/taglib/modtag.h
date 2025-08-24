/***************************************************************************
    copyright           : (C) 2011 by Mathias Panzenböck
    email               : grosser.meister.morti@gmx.net
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

#ifndef TAGLIB_MODTAG_H
#define TAGLIB_MODTAG_H

#include "tag.h"

namespace TagLib {
  namespace Mod {

    //! A module file tag implementation

    /*!
     * Tags for module files (Mod, S3M, IT, XM).
     *
     * Note that only the \a title is supported as such by most
     * module file formats.  Except for XM files the \a trackerName
     * is derived from the file format or the flavour of the file
     * format.  For XM files it is stored in the file.
     *
     * The \a comment tag is not strictly supported by module files,
     * but it is common practice to abuse instrument/sample/pattern
     * names as multiline comments.  TagLib does so as well.
     */
    class TAGLIB_EXPORT Tag : public TagLib::Tag
    {
    public:
      Tag();
      ~Tag() override;

      Tag(const Tag &) = delete;
      Tag &operator=(const Tag &) = delete;

      /*!
       * Returns the track name; if no track name is present in the tag
       * an empty string will be returned.
       */
      String title() const override;

      /*!
       * Not supported by module files.  Therefore always returns an empty string.
       */
      String artist() const override;

      /*!
       * Not supported by module files.  Therefore always returns an empty string.
       */
      String album() const override;

      /*!
       * Returns the track comment derived from the instrument/sample/pattern
       * names; if no comment is present in the tag an empty string will be
       * returned.
       */
      String comment() const override;

      /*!
       * Not supported by module files.  Therefore always returns an empty string.
       */
      String genre() const override;

      /*!
       * Not supported by module files.  Therefore always returns 0.
       */
      unsigned int year() const override;

      /*!
       * Not supported by module files.  Therefore always returns 0.
       */
      unsigned int track() const override;

      /*!
       * Returns the name of the tracker used to create/edit the module file.
       * Only XM files store this tag to the file as such, for other formats
       * (Mod, S3M, IT) this is derived from the file type or the flavour of
       * the file type.  Therefore only XM files might have an empty
       * tracker name.
       */
      String trackerName() const;

      /*!
       * Sets the title to \a title.  If \a title is an empty string then this
       * value will be cleared.
       *
       * The length limits per file type are (1 character = 1 byte):
       * Mod 20 characters, S3M 27 characters, IT 25 characters and XM 20
       * characters.
       */
      void setTitle(const String &title) override;

      /*!
       * Not supported by module files and therefore ignored.
       */
      void setArtist(const String &artist) override;

      /*!
       * Not supported by module files and therefore ignored.
       */
      void setAlbum(const String &album) override;

      /*!
       * Sets the comment to \a comment.  If \a comment is an empty string then
       * this value will be cleared.
       *
       * Note that module file formats don't actually support a comment tag.
       * Instead the names of instruments/patterns/samples are abused as
       * a multiline comment. Because of this the number of lines in a
       * module file is fixed to the number of instruments/patterns/samples.
       *
       * Also note that the instrument/pattern/sample name length is limited
       * and thus the line length in comments are limited. Too big comments
       * will be truncated.
       *
       * The line length limits per file type are (1 character = 1 byte):
       * Mod 22 characters, S3M 27 characters, IT 25 characters and XM 22
       * characters.
       */
      void setComment(const String &comment) override;

      /*!
       * Not supported by module files and therefore ignored.
       */
      void setGenre(const String &genre) override;

      /*!
       * Not supported by module files and therefore ignored.
       */
      void setYear(unsigned int year) override;

      /*!
       * Not supported by module files and therefore ignored.
       */
      void setTrack(unsigned int track) override;

      /*!
       * Sets the tracker name to \a trackerName.  If \a trackerName is
       * an empty string then this value will be cleared.
       *
       * Note that only XM files support this tag.  Setting the
       * tracker name for other module file formats will be ignored.
       *
       * The length of this tag is limited to 20 characters (1 character
       * = 1 byte).
       */
      void setTrackerName(const String &trackerName);

      /*!
       * Implements the unified property interface -- export function.
       * Since the module tag is very limited, the exported map is as well.
       */
      PropertyMap properties() const override;

      /*!
       * Implements the unified property interface -- import function.
       * Because of the limitations of the module file tag, any tags besides
       * COMMENT, TITLE and, if it is an XM file, TRACKERNAME, will be
       * returned. Additionally, if the map contains tags with multiple values,
       * all but the first will be contained in the returned map of unsupported
       * properties.
       */
      PropertyMap setProperties(const PropertyMap &) override;

    private:
      class TagPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<TagPrivate> d;
    };
  }  // namespace Mod
}  // namespace TagLib
#endif
