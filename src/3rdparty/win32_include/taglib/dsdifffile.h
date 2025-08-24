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

#ifndef TAGLIB_DSDIFFFILE_H
#define TAGLIB_DSDIFFFILE_H

#include "tfile.h"
#include "id3v2tag.h"
#include "dsdiffproperties.h"
#include "dsdiffdiintag.h"

namespace TagLib {

  //! An implementation of DSDIFF metadata

  /*!
   * This is an implementation of DSDIFF metadata.
   *
   * This supports an ID3v2 tag as well as reading stream from the ID3 RIFF
   * chunk as well as properties from the file.
   * Description of the DSDIFF format is available at
   * <a href="https://dsd-guide.com/sites/default/files/white-papers/DSDIFF_1.5_Spec.pdf">
   * DSDIFF_1.5_Spec.pdf</a>.
   * The DSDIFF standard does not explicitly specify the ID3 chunk.
   * It can be found at the root level, but also sometimes inside the PROP chunk.
   * In addition, title and artist info are stored as part of the standard.
   */

  namespace DSDIFF {

    //! An implementation of TagLib::File with DSDIFF specific methods.

    /*!
     * This implements and provides an interface for DSDIFF files to the
     * TagLib::Tag and TagLib::AudioProperties interfaces by way of implementing
     * the abstract TagLib::File API as well as providing some additional
     * information specific to DSDIFF files.
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
        //! Matches DIIN tags.
        DIIN    = 0x0001,
        //! Matches ID3v2 tags.
        ID3v2   = 0x0002,
        //! Matches all tag types.
        AllTags = 0xffff
      };

      /*!
       * Constructs a DSDIFF file from \a file.  If \a readProperties is \c true
       * the file's audio properties will also be read.
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
       * Constructs a DSDIFF file from \a stream.  If \a readProperties is \c true
       * the file's audio properties will also be read.
       *
       * If this file contains an ID3v2 tag, the frames will be created using
       * \a frameFactory (default if null).
       *
       * \note TagLib will *not* take ownership of the stream, the caller is
       * responsible for deleting it after the File object.
       *
       * \note In the current implementation, \a propertiesStyle is ignored.
       */
      File(IOStream *stream, bool readProperties = true,
           Properties::ReadStyle propertiesStyle = Properties::Average,
           ID3v2::FrameFactory *frameFactory = nullptr);

      /*!
       * Destroys this instance of the File.
       */
      ~File() override;

      /*!
       * Returns a pointer to a tag that is the union of the ID3v2 and DIIN
       * tags. The ID3v2 tag is given priority in reading the information -- if
       * requested information exists in both the ID3v2 tag and the ID3v1 tag,
       * the information from the ID3v2 tag will be returned.
       *
       * If you would like more granular control over the content of the tags,
       * with the concession of generality, use the tag-type specific calls.
       *
       * \note As this tag is not implemented as an ID3v2 tag or a DIIN tag,
       * but a union of the two this pointer may not be cast to the specific
       * tag types.
       *
       * \see ID3v2Tag()
       * \see DIINTag()
       */
      Tag *tag() const override;

      /*!
       * Returns the ID3V2 Tag for this file.
       *
       * \note This always returns a valid pointer regardless of whether or not
       * the file on disk has an ID3v2 tag.  Use hasID3v2Tag() to check if the
       * file on disk actually has an ID3v2 tag.
       *
       * \see hasID3v2Tag()
       */
      ID3v2::Tag *ID3v2Tag(bool create = false) const;

      /*!
       * Returns the DSDIFF DIIN Tag for this file
       *
       */
      DSDIFF::DIIN::Tag *DIINTag(bool create = false) const;

      /*!
       * Implements the unified property interface -- export function.
       * This method forwards to ID3v2::Tag::properties().
       */
      PropertyMap properties() const override;

      void removeUnsupportedProperties(const StringList &properties) override;

      /*!
       * Implements the unified property interface -- import function.
       * This method forwards to ID3v2::Tag::setProperties().
       */
      PropertyMap setProperties(const PropertyMap &) override;

      /*!
       * Returns the AIFF::Properties for this file.  If no audio properties
       * were read then this will return a null pointer.
       */
      Properties *audioProperties() const override;

      /*!
       * Save the file.  If at least one tag -- ID3v1 or DIIN -- exists this
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
       * Save the file.  If \a strip is specified, it is possible to choose if
       * tags not specified in \a tags should be stripped from the file or
       * retained.  With \a version, it is possible to specify whether ID3v2.4
       * or ID3v2.3 should be used.
       */
      bool save(int tags, StripTags strip = StripOthers, ID3v2::Version version = ID3v2::v4);

      /*!
       * This will strip the tags that match the OR-ed together TagTypes from the
       * file.  By default it strips all tags.  It returns \c true if the tags are
       * successfully stripped.
       *
       * \note This will update the file immediately.
       */
      void strip(int tags = AllTags);

      /*!
       * Returns whether or not the file on disk actually has an ID3v2 tag.
       *
       * \see ID3v2Tag()
       */
      bool hasID3v2Tag() const;

      /*!
       * Returns whether or not the file on disk actually has the DSDIFF
       * title and artist tags.
       *
       * \see DIINTag()
       */
      bool hasDIINTag() const;

      /*!
       * Returns whether or not the given \a stream can be opened as a DSDIFF
       * file.
       *
       * \note This method is designed to do a quick check.  The result may
       * not necessarily be correct.
       */
       static bool isSupported(IOStream *stream);

    protected:
      enum Endianness { BigEndian, LittleEndian };

    private:
      void removeRootChunk(const ByteVector &id);
      void removeRootChunk(unsigned int i);
      void removeChildChunk(const ByteVector &id, unsigned int childChunkNum);
      void removeChildChunk(unsigned int i, unsigned int childChunkNum);

      /*!
       * Sets the data for the specified chunk at root level to \a data.
       *
       * \warning This will update the file immediately.
       */
      void setRootChunkData(unsigned int i, const ByteVector &data);

      /*!
       * Sets the data for the root-level chunk \a name to \a data.
       * If a root-level chunk with the given name already exists
       * it will be overwritten, otherwise it will be
       * created after the existing chunks.
       *
       * \warning This will update the file immediately.
       */
      void setRootChunkData(const ByteVector &name, const ByteVector &data);

      /*!
       * Sets the data for the specified child chunk to \a data.
       *
       * If data is null, then remove the chunk
       *
       * \warning This will update the file immediately.
       */
      void setChildChunkData(unsigned int i, const ByteVector &data,
                             unsigned int childChunkNum);

      /*!
       * Sets the data for the child chunk \a name to \a data.  If a chunk with
       * the given name already exists it will be overwritten, otherwise it will
       * be created after the existing chunks inside the child chunk.
       *
       * If data is null, then remove the chunks with \a name name
       *
       * \warning This will update the file immediately.
       */
      void setChildChunkData(const ByteVector &name, const ByteVector &data,
                             unsigned int childChunkNum);

      void updateRootChunksStructure(unsigned int startingChunk);

      void read(bool readProperties, Properties::ReadStyle propertiesStyle);
      void writeChunk(const ByteVector &name, const ByteVector &data,
                      unsigned long long offset, unsigned long replace = 0,
                      unsigned int leadingPadding = 0);

      class FilePrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<FilePrivate> d;
    };
  }  // namespace DSDIFF
}  // namespace TagLib

#endif
