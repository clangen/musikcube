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

#ifndef TAGLIB_XMFILE_H
#define TAGLIB_XMFILE_H

#include "tfile.h"
#include "audioproperties.h"
#include "taglib_export.h"
#include "modfilebase.h"
#include "modtag.h"
#include "xmproperties.h"

namespace TagLib {

  namespace XM {

    class TAGLIB_EXPORT File : public Mod::FileBase {
      public:
        /*!
         * Constructs an Extended Module file from \a file.
         *
         * \note In the current implementation, both \a readProperties and
         * \a propertiesStyle are ignored.  The audio properties are always
         * read.
         */
        File(FileName file, bool readProperties = true,
             AudioProperties::ReadStyle propertiesStyle =
             AudioProperties::Average);

        /*!
         * Constructs an Extended Module file from \a stream.
         *
         * \note In the current implementation, both \a readProperties and
         * \a propertiesStyle are ignored.  The audio properties are always
         * read.
         *
         * \note TagLib will *not* take ownership of the stream, the caller is
         * responsible for deleting it after the File object.
         */
        File(IOStream *stream, bool readProperties = true,
             AudioProperties::ReadStyle propertiesStyle =
             AudioProperties::Average);

        /*!
         * Destroys this instance of the File.
         */
        virtual ~File();

        Mod::Tag *tag() const;

        /*!
         * Implements the unified property interface -- export function.
         * Forwards to Mod::Tag::properties().
         */
        PropertyMap properties() const;

        /*!
         * Implements the unified property interface -- import function.
         * Forwards to Mod::Tag::setProperties().
         */
        PropertyMap setProperties(const PropertyMap &);

        /*!
         * Returns the XM::Properties for this file. If no audio properties
         * were read then this will return a null pointer.
         */
        XM::Properties *audioProperties() const;

        /*!
         * Save the file.
         * This is the same as calling save(AllTags);
         *
         * \note Saving Extended Module tags is not supported.
         */
        bool save();

      private:
        File(const File &);
        File &operator=(const File &);

        void read(bool readProperties);

        class FilePrivate;
        FilePrivate *d;
    };
  }
}

#endif
