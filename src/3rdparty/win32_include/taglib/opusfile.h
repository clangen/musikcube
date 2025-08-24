/***************************************************************************
    copyright            : (C) 2012 by Lukáš Lalinský
    email                : lalinsky@gmail.com

    copyright            : (C) 2002 - 2008 by Scott Wheeler
    email                : wheeler@kde.org
                           (original Vorbis implementation)
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

#ifndef TAGLIB_OPUSFILE_H
#define TAGLIB_OPUSFILE_H

#include "oggfile.h"
#include "xiphcomment.h"
#include "opusproperties.h"

namespace TagLib {

  namespace Ogg {

    //! A namespace containing classes for Opus metadata

    namespace Opus {

      //! An implementation of Ogg::File with Opus specific methods

      /*!
       * This is the central class in the Ogg Opus metadata processing collection
       * of classes.  It's built upon Ogg::File which handles processing of the Ogg
       * logical bitstream and breaking it down into pages which are handled by
       * the codec implementations, in this case Opus specifically.
       */

      class TAGLIB_EXPORT File : public Ogg::File
      {
      public:
        /*!
         * Constructs an Opus file from \a file.  If \a readProperties is \c true the
         * file's audio properties will also be read.
         *
         * \note In the current implementation, \a propertiesStyle is ignored.
         */
        File(FileName file, bool readProperties = true,
             Properties::ReadStyle propertiesStyle = Properties::Average);

        /*!
         * Constructs an Opus file from \a stream.  If \a readProperties is \c true the
         * file's audio properties will also be read.
         *
         * \note TagLib will *not* take ownership of the stream, the caller is
         * responsible for deleting it after the File object.
         *
         * \note In the current implementation, \a propertiesStyle is ignored.
         */
        File(IOStream *stream, bool readProperties = true,
             Properties::ReadStyle propertiesStyle = Properties::Average);

        /*!
         * Destroys this instance of the File.
         */
        ~File() override;

        File(const File &) = delete;
        File &operator=(const File &) = delete;

        /*!
         * Returns the XiphComment for this file.  XiphComment implements the tag
         * interface, so this serves as the reimplementation of
         * TagLib::File::tag().
         */
        Ogg::XiphComment *tag() const override;

        /*!
         * Implements the unified property interface -- export function.
         * This forwards directly to XiphComment::properties().
         */
        PropertyMap properties() const override;

        /*!
         * Implements the unified tag dictionary interface -- import function.
         * Like properties(), this is a forwarder to the file's XiphComment.
         */
        PropertyMap setProperties(const PropertyMap &) override;

        /*!
         * Returns the Opus::Properties for this file.  If no audio properties
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
         * Returns whether or not the given \a stream can be opened as an Opus
         * file.
         *
         * \note This method is designed to do a quick check.  The result may
         * not necessarily be correct.
         */
        static bool isSupported(IOStream *stream);

      private:
        void read(bool readProperties);

        class FilePrivate;
        TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
        std::unique_ptr<FilePrivate> d;
      };
    }  // namespace Opus
  }  // namespace Ogg
}  // namespace TagLib

#endif
