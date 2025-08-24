/***************************************************************************
    copyright            : (C) 2016 by Damien Plisson, Audirvana
    email                : damien78@audirvana.com
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

#ifndef TAGLIB_DSDIFFDIINTAG_H
#define TAGLIB_DSDIFFDIINTAG_H

#include "tag.h"

namespace TagLib {

  namespace DSDIFF {

    namespace DIIN {

      /*!
       * Tags from the Edited Master Chunk Info
       *
       * Only Title and Artist tags are supported
       */
      class TAGLIB_EXPORT Tag : public TagLib::Tag
      {
      public:
        Tag();
        ~Tag() override;

        /*!
         * Returns the track name; if no track name is present in the tag
         * String() will be returned.
         */
        String title() const override;

        /*!
         * Returns the artist name; if no artist name is present in the tag
         * String() will be returned.
         */
        String artist() const override;

        /*!
         * Not supported.  Therefore always returns String().
         */
        String album() const override;

        /*!
         * Not supported.  Therefore always returns String().
         */
        String comment() const override;

        /*!
         * Not supported.  Therefore always returns String().
         */
        String genre() const override;

        /*!
         * Not supported.  Therefore always returns 0.
         */
        unsigned int year() const override;

        /*!
         * Not supported.  Therefore always returns 0.
         */
        unsigned int track() const override;

        /*!
         * Sets the title to \a title.  If \a title is String() then this
         * value will be cleared.
         */
        void setTitle(const String &title) override;

        /*!
         * Sets the artist to \a artist.  If \a artist is String() then this
         * value will be cleared.
         */
        void setArtist(const String &artist) override;

        /*!
         * Not supported and therefore ignored.
         */
        void setAlbum(const String &album) override;

        /*!
         * Not supported and therefore ignored.
         */
        void setComment(const String &comment) override;

        /*!
         * Not supported and therefore ignored.
         */
        void setGenre(const String &genre) override;

        /*!
         * Not supported and therefore ignored.
         */
        void setYear(unsigned int year) override;

        /*!
         * Not supported and therefore ignored.
         */
        void setTrack(unsigned int track) override;

        /*!
         * Implements the unified property interface -- export function.
         * Since the DIIN tag is very limited, the exported map is as well.
         */
        PropertyMap properties() const override;

        /*!
         * Implements the unified property interface -- import function.
         * Because of the limitations of the DIIN file tag, any tags besides
         * TITLE and ARTIST, will be
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
    }  // namespace DIIN
  }  // namespace DSDIFF
}  // namespace TagLib

#endif
