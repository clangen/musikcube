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

#ifndef TAGLIB_MPEGFILE_H
#define TAGLIB_MPEGFILE_H

#include "tfile.h"
#include "taglib_export.h"
#include "tag.h"
#include "mpegproperties.h"
#include "id3v2.h"

namespace TagLib {

  namespace ID3v2 { class Tag; class FrameFactory; }
  namespace ID3v1 { class Tag; }
  namespace APE { class Tag; }

  //! An implementation of TagLib::File with MPEG (MP3) specific methods

  namespace MPEG {

    //! An MPEG file class with some useful methods specific to MPEG

    /*!
     * This implements the generic TagLib::File API and additionally provides
     * access to properties that are distinct to MPEG files, notably access
     * to the different ID3 tags.
     */

    class TAGLIB_EXPORT File : public TagLib::File
    {
    public:
      /*!
       * This set of flags is used for various operations and is suitable for
       * being OR-ed together.
       */
      enum TagTypes {
        //! Empty set.  Matches no tag types.
        NoTags  = 0x0000,
        //! Matches ID3v1 tags.
        ID3v1   = 0x0001,
        //! Matches ID3v2 tags.
        ID3v2   = 0x0002,
        //! Matches APE tags.
        APE     = 0x0004,
        //! Matches all tag types.
        AllTags = 0xffff
      };

      /*!
       * Constructs an MPEG file from \a file.  If \a readProperties is \c true the
       * file's audio properties will also be read.
       *
       * If \a readStyle is not Fast, the file will be scanned
       * completely if no ID3v2 tag or MPEG sync code is found at the start.
       *
       * If this file contains an ID3v2 tag, the frames will be created using
       * \a frameFactory (default if null).
       */
      File(FileName file, bool readProperties = true,
           Properties::ReadStyle readStyle = Properties::Average,
           ID3v2::FrameFactory *frameFactory = nullptr);

      /*!
       * Constructs an MPEG file from \a file.  If \a readProperties is \c true the
       * file's audio properties will also be read.
       *
       * If this file contains an ID3v2 tag, the frames will be created using
       * \a frameFactory.
       *
       * If \a readStyle is not Fast, the file will be scanned
       * completely if no ID3v2 tag or MPEG sync code is found at the start.
       *
       * \deprecated Use the constructor above.
       */
      TAGLIB_DEPRECATED
      File(FileName file, ID3v2::FrameFactory *frameFactory,
           bool readProperties = true,
           Properties::ReadStyle readStyle = Properties::Average);

      /*!
       * Constructs an MPEG file from \a stream.  If \a readProperties is \c true the
       * file's audio properties will also be read.
       *
       * \note TagLib will *not* take ownership of the stream, the caller is
       * responsible for deleting it after the File object.
       *
       * If this file contains an ID3v2 tag, the frames will be created using
       * \a frameFactory.
       *
       * If \a readStyle is not Fast, the file will be scanned
       * completely if no ID3v2 tag or MPEG sync code is found at the start.
       *
       * If this file contains an ID3v2 tag, the frames will be created using
       * \a frameFactory (default if null).
       */
      File(IOStream *stream, bool readProperties = true,
           Properties::ReadStyle readStyle = Properties::Average,
           ID3v2::FrameFactory *frameFactory = nullptr);

      /*!
       * Constructs an MPEG file from \a stream.  If \a readProperties is \c true the
       * file's audio properties will also be read.
       *
       * \note TagLib will *not* take ownership of the stream, the caller is
       * responsible for deleting it after the File object.
       *
       * If this file contains an ID3v2 tag, the frames will be created using
       * \a frameFactory.
       *
       * If \a readStyle is not Fast, the file will be scanned
       * completely if no ID3v2 tag or MPEG sync code is found at the start.
       *
       * \deprecated Use the constructor above.
       */
      TAGLIB_DEPRECATED
      File(IOStream *stream, ID3v2::FrameFactory *frameFactory,
           bool readProperties = true,
           Properties::ReadStyle readStyle = Properties::Average);

      /*!
       * Destroys this instance of the File.
       */
      ~File() override;

      File(const File &) = delete;
      File &operator=(const File &) = delete;

      /*!
       * Returns a pointer to a tag that is the union of the ID3v2 and ID3v1
       * tags. The ID3v2 tag is given priority in reading the information -- if
       * requested information exists in both the ID3v2 tag and the ID3v1 tag,
       * the information from the ID3v2 tag will be returned.
       *
       * If you would like more granular control over the content of the tags,
       * with the concession of generality, use the tag-type specific calls.
       *
       * \note As this tag is not implemented as an ID3v2 tag or an ID3v1 tag,
       * but a union of the two this pointer may not be cast to the specific
       * tag types.
       *
       * \see ID3v1Tag()
       * \see ID3v2Tag()
       * \see APETag()
       */
      Tag *tag() const override;

      /*!
       * Implements the reading part of the unified property interface.
       * If the file contains more than one tag, only the
       * first one (in the order ID3v2, APE, ID3v1) will be converted to the
       * PropertyMap.
       */
      PropertyMap properties() const override;

      void removeUnsupportedProperties(const StringList &properties) override;

      /*!
       * Implements the writing part of the unified tag dictionary interface.
       * In order to avoid problems with deprecated tag formats, this method
       * always creates an ID3v2 tag if necessary.
       * If an ID3v1 tag  exists, it will be updated as well, within the
       * limitations of that format.
       * The returned PropertyMap refers to the ID3v2 tag only.
       */
      PropertyMap setProperties(const PropertyMap &) override;

      /*!
       * Returns the MPEG::Properties for this file.  If no audio properties
       * were read then this will return a null pointer.
       */
      Properties *audioProperties() const override;

      /*!
       * Save the file.  If at least one tag -- ID3v1 or ID3v2 -- exists this
       * will duplicate its content into the other tag.  This returns \c true
       * if saving was successful.
       *
       * If neither exists or if both tags are empty, this will strip the tags
       * from the file.
       *
       * This is the same as calling save(AllTags);
       *
       * If you would like more granular control over the content of the tags,
       * with the concession of generality, use parameterized save call below.
       *
       * \see save(int tags)
       */
      bool save() override;

      /*!
       * Save the file.  This will attempt to save all of the tag types that are
       * specified by OR-ing together TagTypes values.
       *
       * \a strip can be set to strip all tags except those in \a tags.  Those
       * tags will not be modified in memory, and thus remain valid.
       *
       * \a version specifies the ID3v2 version to be used for writing tags.  By
       * default, the latest standard, ID3v2.4 is used.
       *
       * If \a duplicate is set to Duplicate and at least one tag -- ID3v1
       * or ID3v2 -- exists this will duplicate its content into the other tag.
       */
      bool save(int tags, StripTags strip = StripOthers,
                ID3v2::Version version = ID3v2::v4,
                DuplicateTags duplicate = Duplicate);

      /*!
       * Returns a pointer to the ID3v2 tag of the file.
       *
       * If \a create is \c false (the default) this may return a null pointer
       * if there is no valid ID3v2 tag.  If \a create is \c true it will create
       * an ID3v2 tag if one does not exist and returns a valid pointer.
       *
       * \note This may return a valid pointer regardless of whether or not the
       * file on disk has an ID3v2 tag.  Use hasID3v2Tag() to check if the file
       * on disk actually has an ID3v2 tag.
       *
       * \note The Tag <b>is still</b> owned by the MPEG::File and should not be
       * deleted by the user.  It will be deleted when the file (object) is
       * destroyed.
       *
       * \see hasID3v2Tag()
       */
      ID3v2::Tag *ID3v2Tag(bool create = false);

      /*!
       * Returns a pointer to the ID3v1 tag of the file.
       *
       * If \a create is \c false (the default) this may return a null pointer
       * if there is no valid ID3v1 tag.  If \a create is \c true it will create
       * an ID3v1 tag if one does not exist and returns a valid pointer.
       *
       * \note This may return a valid pointer regardless of whether or not the
       * file on disk has an ID3v1 tag.  Use hasID3v1Tag() to check if the file
       * on disk actually has an ID3v1 tag.
       *
       * \note The Tag <b>is still</b> owned by the MPEG::File and should not be
       * deleted by the user.  It will be deleted when the file (object) is
       * destroyed.
       *
       * \see hasID3v1Tag()
       */
      ID3v1::Tag *ID3v1Tag(bool create = false);

      /*!
       * Returns a pointer to the APE tag of the file.
       *
       * If \a create is \c false (the default) this may return a null pointer
       * if there is no valid APE tag.  If \a create is \c true it will create
       * an APE tag if one does not exist and returns a valid pointer.
       *
       * \note This may return a valid pointer regardless of whether or not the
       * file on disk has an APE tag.  Use hasAPETag() to check if the file
       * on disk actually has an APE tag.
       *
       * \note The Tag <b>is still</b> owned by the MPEG::File and should not be
       * deleted by the user.  It will be deleted when the file (object) is
       * destroyed.
       *
       * \see hasAPETag()
       */
      APE::Tag *APETag(bool create = false);

      /*!
       * This will strip the tags that match the OR-ed together TagTypes from the
       * file.  By default it strips all tags.  It returns \c true if the tags are
       * successfully stripped.
       *
       * If \a freeMemory is \c true the ID3 and APE tags will be deleted and
       * pointers to them will be invalidated.
       *
       * \note This will update the file immediately.
       */
      bool strip(int tags = AllTags, bool freeMemory = true);

      /*!
       * Returns the position in the file of the first MPEG frame.
       */
      offset_t firstFrameOffset();

      /*!
       * Returns the position in the file of the next MPEG frame,
       * using the current position as start.
       */
      offset_t nextFrameOffset(offset_t position);

      /*!
       * Returns the position in the file of the previous MPEG frame,
       * using the current position as start.
       */
      offset_t previousFrameOffset(offset_t position);

      /*!
       * Returns the position in the file of the last MPEG frame.
       */
      offset_t lastFrameOffset();

      /*!
       * Returns whether or not the file on disk actually has an ID3v1 tag.
       *
       * \see ID3v1Tag()
       */
      bool hasID3v1Tag() const;

      /*!
       * Returns whether or not the file on disk actually has an ID3v2 tag.
       *
       * \see ID3v2Tag()
       */
      bool hasID3v2Tag() const;

      /*!
       * Returns whether or not the file on disk actually has an APE tag.
       *
       * \see APETag()
       */
      bool hasAPETag() const;

      /*!
       * Returns whether or not the given \a stream can be opened as an MPEG
       * file.
       *
       * \note This method is designed to do a quick check.  The result may
       * not necessarily be correct.
       */
      static bool isSupported(IOStream *stream);

    private:
      void read(bool readProperties, Properties::ReadStyle readStyle);
      offset_t findID3v2(Properties::ReadStyle readStyle);

      class FilePrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<FilePrivate> d;
    };
  }  // namespace MPEG
}  // namespace TagLib

#endif
