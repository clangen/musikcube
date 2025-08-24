/***************************************************************************
    copyright            : (C) 2003 by Allan Sandfeld Jensen
    email                : kde@carewolf.org
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

#ifndef TAGLIB_FLACFILE_H
#define TAGLIB_FLACFILE_H

#include "tfile.h"
#include "tlist.h"
#include "taglib_export.h"
#include "tag.h"
#include "flacpicture.h"
#include "flacproperties.h"

namespace TagLib {

  class Tag;
  namespace ID3v2 { class FrameFactory; class Tag; }
  namespace ID3v1 { class Tag; }
  namespace Ogg { class XiphComment; }

  //! An implementation of FLAC metadata

  /*!
   * This is an implementation of FLAC metadata for non-Ogg FLAC files.  At some
   * point when Ogg / FLAC is more common there will be a similar implementation
   * under the Ogg hierarchy.
   *
   * This supports ID3v1, ID3v2 and Xiph style comments as well as reading stream
   * properties from the file.
   */

  namespace FLAC {

    //! An implementation of TagLib::File with FLAC specific methods

    /*!
     * This implements and provides an interface for FLAC files to the
     * TagLib::Tag and TagLib::AudioProperties interfaces by way of implementing
     * the abstract TagLib::File API as well as providing some additional
     * information specific to FLAC files.
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
        NoTags      = 0x0000,
        //! Matches Vorbis comments.
        XiphComment = 0x0001,
        //! Matches ID3v1 tags.
        ID3v1       = 0x0002,
        //! Matches ID3v2 tags.
        ID3v2       = 0x0004,
        //! Matches all tag types.
        AllTags     = 0xffff
      };

      /*!
       * Constructs a FLAC file from \a file.  If \a readProperties is \c true the
       * file's audio properties will also be read.
       *
       * \note In the current implementation, \a propertiesStyle is ignored.
       *
       * If this file contains an ID3v2 tag, the frames will be created using
       * \a frameFactory (default if null).
       */
      File(FileName file, bool readProperties = true,
           Properties::ReadStyle propertiesStyle = Properties::Average,
           ID3v2::FrameFactory *frameFactory = nullptr);

      /*!
       * Constructs a FLAC file from \a file.  If \a readProperties is \c true the
       * file's audio properties will also be read.
       *
       * If this file contains an ID3v2 tag, the frames will be created using
       * \a frameFactory.
       *
       * \note In the current implementation, \a propertiesStyle is ignored.
       *
       * \deprecated Use the constructor above.
       */
      TAGLIB_DEPRECATED
      File(FileName file, ID3v2::FrameFactory *frameFactory,
           bool readProperties = true,
           Properties::ReadStyle propertiesStyle = Properties::Average);

      /*!
       * Constructs a FLAC file from \a stream.  If \a readProperties is \c true the
       * file's audio properties will also be read.
       *
       * \note TagLib will *not* take ownership of the stream, the caller is
       * responsible for deleting it after the File object.
       *
       * If this file contains an ID3v2 tag, the frames will be created using
       * \a frameFactory (default if null).
       *
       * \note In the current implementation, \a propertiesStyle is ignored.
       */
      File(IOStream *stream, bool readProperties = true,
           Properties::ReadStyle propertiesStyle = Properties::Average,
           ID3v2::FrameFactory *frameFactory = nullptr);

      /*!
       * Constructs a FLAC file from \a stream.  If \a readProperties is \c true the
       * file's audio properties will also be read.
       *
       * \note TagLib will *not* take ownership of the stream, the caller is
       * responsible for deleting it after the File object.
       *
       * If this file contains an ID3v2 tag, the frames will be created using
       * \a frameFactory.
       *
       * \note In the current implementation, \a propertiesStyle is ignored.
       *
       * \deprecated Use the constructor above.
       */
      TAGLIB_DEPRECATED
      File(IOStream *stream, ID3v2::FrameFactory *frameFactory,
           bool readProperties = true,
           Properties::ReadStyle propertiesStyle = Properties::Average);

      /*!
       * Destroys this instance of the File.
       */
      ~File() override;

      File(const File &) = delete;
      File &operator=(const File &) = delete;

      /*!
       * Returns the Tag for this file.  This will be a union of XiphComment,
       * ID3v1 and ID3v2 tags.
       *
       * \see ID3v2Tag()
       * \see ID3v1Tag()
       * \see XiphComment()
       */
      TagLib::Tag *tag() const override;

      /*!
       * Implements the unified property interface -- export function.
       * If the file contains more than one tag (e.g. XiphComment and ID3v1),
       * only the first one (in the order XiphComment, ID3v2, ID3v1) will be
       * converted to the PropertyMap.
       */
      PropertyMap properties() const override;

      void removeUnsupportedProperties(const StringList &) override;

      /*!
       * Implements the unified property interface -- import function.
       * This always creates a Xiph comment, if none exists. The return value
       * relates to the Xiph comment only.
       * Ignores any changes to ID3v1 or ID3v2 comments since they are not allowed
       * in the FLAC specification.
       */
      PropertyMap setProperties(const PropertyMap &) override;

      /*!
       * Returns ["PICTURE"] if any picture is stored in METADATA_BLOCK_PICTURE.
       */
      StringList complexPropertyKeys() const override;

      /*!
       * Get the pictures stored in METADATA_BLOCK_PICTURE as complex properties
       * for \a key "PICTURE".
       */
      List<VariantMap> complexProperties(const String &key) const override;

      /*!
       * Set the complex properties \a value as pictures in METADATA_BLOCK_PICTURE
       * for \a key "PICTURE".
       */
      bool setComplexProperties(const String &key, const List<VariantMap> &value) override;

      /*!
       * Returns the FLAC::Properties for this file.  If no audio properties
       * were read then this will return a null pointer.
       */
      Properties *audioProperties() const override;

      /*!
       * Save the file.  This will primarily save the XiphComment, but
       * will also keep any old ID3-tags up to date. If the file
       * has no XiphComment, one will be constructed from the ID3-tags.
       *
       * This returns \c true if the save was successful.
       */
      bool save() override;

      /*!
       * Returns a pointer to the ID3v2 tag of the file.
       *
       * If \a create is \c false (the default) this returns a null pointer
       * if there is no valid ID3v2 tag.  If \a create is \c true it will create
       * an ID3v2 tag if one does not exist and returns a valid pointer.
       *
       * \note This may return a valid pointer regardless of whether or not the
       * file on disk has an ID3v2 tag.  Use hasID3v2Tag() to check if the file
       * on disk actually has an ID3v2 tag.
       *
       * \note The Tag <b>is still</b> owned by the FLAC::File and should not be
       * deleted by the user.  It will be deleted when the file (object) is
       * destroyed.
       *
       * \see hasID3v2Tag()
       */
      ID3v2::Tag *ID3v2Tag(bool create = false);

      /*!
       * Returns a pointer to the ID3v1 tag of the file.
       *
       * If \a create is \c false (the default) this returns a null pointer
       * if there is no valid APE tag.  If \a create is \c true it will create
       * an APE tag if one does not exist and returns a valid pointer.
       *
       * \note This may return a valid pointer regardless of whether or not the
       * file on disk has an ID3v1 tag.  Use hasID3v1Tag() to check if the file
       * on disk actually has an ID3v1 tag.
       *
       * \note The Tag <b>is still</b> owned by the FLAC::File and should not be
       * deleted by the user.  It will be deleted when the file (object) is
       * destroyed.
       *
       * \see hasID3v1Tag()
       */
      ID3v1::Tag *ID3v1Tag(bool create = false);

      /*!
       * Returns a pointer to the XiphComment for the file.
       *
       * If \a create is \c false (the default) this returns a null pointer
       * if there is no valid XiphComment.  If \a create is \c true it will create
       * a XiphComment if one does not exist and returns a valid pointer.
       *
       * \note This may return a valid pointer regardless of whether or not the
       * file on disk has a XiphComment.  Use hasXiphComment() to check if the
       * file on disk actually has a XiphComment.
       *
       * \note The Tag <b>is still</b> owned by the FLAC::File and should not be
       * deleted by the user.  It will be deleted when the file (object) is
       * destroyed.
       *
       * \see hasXiphComment()
       */
      Ogg::XiphComment *xiphComment(bool create = false);

      /*!
       * Returns a list of pictures attached to the FLAC file.
       */
      List<Picture *> pictureList();

      /*!
       * Removes an attached picture. If \a del is \c true the picture's memory
       * will be freed; if it is \c false, it must be deleted by the user.
       */
      void removePicture(Picture *picture, bool del = true);

      /*!
       * Remove all attached images.
       */
      void removePictures();

      /*!
       * Add a new picture to the file. The file takes ownership of the
       * picture and will handle freeing its memory.
       *
       * \note The file will be saved only after calling save().
       */
      void addPicture(Picture *picture);

      /*!
       * This will remove the tags that match the OR-ed together TagTypes from
       * the file.  By default it removes all tags.
       *
       * \warning This will also invalidate pointers to the tags as their memory
       * will be freed.
       *
       * \note In order to make the removal permanent save() still needs to be
       * called.
       *
       * \note This won't remove the Vorbis comment block completely.  The
       * vendor ID will be preserved.
       */
      void strip(int tags = AllTags);

      /*!
       * Returns whether or not the file on disk actually has a XiphComment.
       *
       * \see xiphComment()
       */
      bool hasXiphComment() const;

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
       * Returns whether or not the given \a stream can be opened as a FLAC
       * file.
       *
       * \note This method is designed to do a quick check.  The result may
       * not necessarily be correct.
       */
      static bool isSupported(IOStream *stream);

    private:
      void read(bool readProperties);
      void scan();

      class FilePrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<FilePrivate> d;
    };
  }  // namespace FLAC
}  // namespace TagLib

#endif
