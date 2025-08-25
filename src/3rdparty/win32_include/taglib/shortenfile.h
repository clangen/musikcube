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

#ifndef TAGLIB_SHORTENFILE_H
#define TAGLIB_SHORTENFILE_H

#include <memory>

#include "taglib_export.h"
#include "tfile.h"

#include "shortenproperties.h"
#include "shortentag.h"

namespace TagLib {

  //! An implementation of Shorten metadata

  /*!
   * This is an implementation of Shorten metadata.
   */

  namespace Shorten {

    //! An implementation of \c TagLib::File with Shorten specific methods

    /*!
     * This implements and provides an interface for Shorten files to the
     * \c TagLib::Tag and \c TagLib::AudioProperties interfaces by way of implementing
     * the abstract \c TagLib::File API as well as providing some additional
     * information specific to Shorten files.
     */

    class TAGLIB_EXPORT File : public TagLib::File {
      public:
        /*!
         * Constructs a Shorten file from \a file.
         *
         * \note In the current implementation, both \a readProperties and
         * \a propertiesStyle are ignored.  The audio properties are always
         * read.
         */
        File(FileName file, bool readProperties = true,
             AudioProperties::ReadStyle propertiesStyle =
             AudioProperties::Average);

        /*!
         * Constructs a Shorten file from \a stream.
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
        ~File() override;

        File(const File &) = delete;
        File &operator=(const File &) = delete;

        /*!
         * Returns the \c Shorten::Tag for this file.
         *
         * \note While the returned \c Tag instance is non-null Shorten tags are not supported.
         */
        Tag *tag() const override;

        /*!
         * Implements the unified property interface -- export function.
         */
        PropertyMap properties() const override;

        /*!
         * Implements the unified property interface -- import function.
         */
        PropertyMap setProperties(const PropertyMap &) override;

        /*!
         * Returns the \c Shorten::Properties for this file. If no audio properties
         * were read then this will return a null pointer.
         */
        Properties *audioProperties() const override;

        /*!
         * Save the file.
         *
         * \note Saving Shorten tags is not supported.
         */
        bool save() override;

        /*!
         * Returns whether or not the given \a stream can be opened as a Shorten
         * file.
         *
         * \note This method is designed to do a quick check.  The result may
         * not necessarily be correct.
         */
        static bool isSupported(IOStream *stream);

      private:
        void read(AudioProperties::ReadStyle propertiesStyle);

        class FilePrivate;
        TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
        std::unique_ptr<FilePrivate> d;
    };
  }  // namespace Shorten
}  // namespace TagLib

#endif
