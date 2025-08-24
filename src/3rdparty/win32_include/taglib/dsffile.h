/***************************************************************************
    copyright           : (C) 2013-2023 Stephen F. Booth
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

#ifndef TAGLIB_DSFFILE_H
#define TAGLIB_DSFFILE_H

#include <memory>

#include "taglib_export.h"
#include "tfile.h"

#include "dsfproperties.h"

#include "id3v2tag.h"

namespace TagLib {

  //! An implementation of DSF metadata

  /*!
   * This is an implementation of DSF metadata using an ID3v2 tag inside the
   * metadata chunk.
   * The DSF specification is located at
   * http://dsd-guide.com/sites/default/files/white-papers/DSFFileFormatSpec_E.pdf
   */

  namespace DSF {

    //! An implementation of TagLib::File with DSF specific methods

    /*!
     * This implements and provides an interface for DSF files to the
     * TagLib::Tag and TagLib::AudioProperties interfaces by way of implementing
     * the abstract TagLib::File API as well as providing some additional
     * information specific to DSF files.
     */

    class TAGLIB_EXPORT File : public TagLib::File {
      public:
        /*!
         * Constructs a DSD stream file from \a file.
         *
         * \note In the current implementation, both \a readProperties and
         * \a propertiesStyle are ignored.  The audio properties are always
         * read.
         *
         * If this file contains an ID3v2 tag, the frames will be created using
         * \a frameFactory (default if null).
         */
        File(FileName file, bool readProperties = true,
             AudioProperties::ReadStyle propertiesStyle =
             AudioProperties::Average,
             ID3v2::FrameFactory *frameFactory = nullptr);

        /*!
         * Constructs a DSD stream file from \a stream.
         *
         * \note In the current implementation, both \a readProperties and
         * \a propertiesStyle are ignored.  The audio properties are always
         * read.
         *
         * If this file contains an ID3v2 tag, the frames will be created using
         * \a frameFactory (default if null).
         *
         * \note TagLib will *not* take ownership of the stream, the caller is
         * responsible for deleting it after the File object.
         */
        File(IOStream *stream, bool readProperties = true,
             AudioProperties::ReadStyle propertiesStyle =
             AudioProperties::Average,
             ID3v2::FrameFactory *frameFactory = nullptr);

        /*!
         * Destroys this instance of the File.
         */
        ~File() override;

        /*!
         * Returns the ID3v2 Tag for this file.
         */
        ID3v2::Tag *tag() const override;

        /*!
         * Implements the unified property interface -- export function.
         * Forwards to ID3v2::Tag::properties().
         */
        PropertyMap properties() const override;

        /*!
         * Implements the unified property interface -- import function.
         * Forwards to ID3v2::Tag::setProperties().
         */
        PropertyMap setProperties(const PropertyMap &) override;

        /*!
         * Returns the DSF::Properties for this file. If no audio properties
         * were read then this will return a null pointer.
         */
        Properties *audioProperties() const override;

        /*!
         * Save the file.
         *
         * This returns \c true if the save was successful.
         */
        bool save() override;

        /*!
         * Save the file.
         *
         * \a version specifies the ID3v2 version to be used for writing tags.
         */
        bool save(ID3v2::Version version);

        /*!
         * Returns whether or not the given \a stream can be opened as a DSF
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
  }  // namespace DSF
}  // namespace TagLib

#endif
