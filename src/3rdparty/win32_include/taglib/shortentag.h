/***************************************************************************
 copyright           : (C) 2020-2024 Stephen F. Booth
 email               : me@sbooth.org
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

#ifndef TAGLIB_SHORTENTAG_H
#define TAGLIB_SHORTENTAG_H

#include "tag.h"

namespace TagLib {
  namespace Shorten {

    //! A Shorten file tag implementation

    /*!
     * Tags for Shorten files.
     *
     * This is a stub class; Shorten files do not support tags.
     */
    class TAGLIB_EXPORT Tag : public TagLib::Tag
    {
    public:
      Tag();
      ~Tag() override;

      Tag(const Tag &) = delete;
      Tag &operator=(const Tag &) = delete;

      /*!
       * Not supported by Shorten files.  Therefore always returns an empty string.
       */
      String title() const override;

      /*!
       * Not supported by Shorten files.  Therefore always returns an empty string.
       */
      String artist() const override;

      /*!
       * Not supported by Shorten files.  Therefore always returns an empty string.
       */
      String album() const override;

      /*!
       * Not supported by Shorten files.  Therefore always returns an empty string.
       */
      String comment() const override;

      /*!
       * Not supported by Shorten files.  Therefore always returns an empty string.
       */
      String genre() const override;

      /*!
       * Not supported by Shorten files.  Therefore always returns 0.
       */
      unsigned int year() const override;

      /*!
       * Not supported by Shorten files.  Therefore always returns 0.
       */
      unsigned int track() const override;

      /*!
       * Not supported by Shorten files and therefore ignored.
       */
      void setTitle(const String &title) override;

      /*!
       * Not supported by Shorten files and therefore ignored.
       */
      void setArtist(const String &artist) override;

      /*!
       * Not supported by Shorten files and therefore ignored.
       */
      void setAlbum(const String &album) override;

      /*!
       * Not supported by Shorten files and therefore ignored.
       */
      void setComment(const String &comment) override;

      /*!
       * Not supported by Shorten files and therefore ignored.
       */
      void setGenre(const String &genre) override;

      /*!
       * Not supported by Shorten files and therefore ignored.
       */
      void setYear(unsigned int year) override;

      /*!
       * Not supported by Shorten files and therefore ignored.
       */
      void setTrack(unsigned int track) override;

      /*!
       * Implements the unified property interface -- export function.
       */
      PropertyMap properties() const override;

      /*!
       * Implements the unified property interface -- import function.
       */
      PropertyMap setProperties(const PropertyMap &) override;

    private:
      class TagPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<TagPrivate> d;
    };

  }  // namespace Shorten
}  // namespace TagLib

#endif
