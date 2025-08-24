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

#ifndef TAGLIB_VORBISCOMMENT_H
#define TAGLIB_VORBISCOMMENT_H

#include "tlist.h"
#include "tmap.h"
#include "tstring.h"
#include "tstringlist.h"
#include "tbytevector.h"
#include "taglib_export.h"
#include "tag.h"
#include "flacpicture.h"

#ifdef _MSC_VER
// Explained at end of tpropertymap.cpp
extern template class TagLib::Map<TagLib::String, TagLib::StringList>;
#endif

namespace TagLib {

  namespace Ogg {

    /*!
     * A mapping between a list of field names, or keys, and a list of values
     * associated with that field.
     *
     * \see XiphComment::fieldListMap()
     */
    using FieldListMap = Map<String, StringList>;

    //! Ogg Vorbis comment implementation

    /*!
     * This class is an implementation of the Ogg Vorbis comment specification,
     * to be found in section 5 of the Ogg Vorbis specification.  Because this
     * format is also used in other (currently unsupported) Xiph.org formats, it
     * has been made part of a generic implementation rather than being limited
     * to strictly Vorbis.
     *
     * Vorbis comments are a simple vector of keys and values, called fields.
     * Multiple values for a given key are supported.
     *
     * \see fieldListMap()
     */

    class TAGLIB_EXPORT XiphComment : public TagLib::Tag
    {
    public:
      /*!
       * Constructs an empty Vorbis comment.
       */
      XiphComment();

      /*!
       * Constructs a Vorbis comment from \a data.
       */
      XiphComment(const ByteVector &data);

      /*!
       * Destroys this instance of the XiphComment.
       */
      ~XiphComment() override;

      XiphComment(const XiphComment &) = delete;
      XiphComment &operator=(const XiphComment &) = delete;

      String title() const override;
      String artist() const override;
      String album() const override;
      String comment() const override;
      String genre() const override;
      unsigned int year() const override;
      unsigned int track() const override;

      void setTitle(const String &s) override;
      void setArtist(const String &s) override;
      void setAlbum(const String &s) override;
      void setComment(const String &s) override;
      void setGenre(const String &s) override;
      void setYear(unsigned int i) override;
      void setTrack(unsigned int i) override;

      bool isEmpty() const override;

      /*!
       * Returns the number of fields present in the comment.
       */
      unsigned int fieldCount() const;

      /*!
       * Returns a reference to the map of field lists.  Because Xiph comments
       * support multiple fields with the same key, a pure Map would not work.
       * As such this is a Map of string lists, keyed on the comment field name.
       *
       * The standard set of Xiph/Vorbis fields (which may or may not be
       * contained in any specific comment) is:
       *
       * <ul>
       *   <li>TITLE</li>
       *   <li>VERSION</li>
       *   <li>ALBUM</li>
       *   <li>ARTIST</li>
       *   <li>PERFORMER</li>
       *   <li>COPYRIGHT</li>
       *   <li>ORGANIZATION</li>
       *   <li>DESCRIPTION</li>
       *   <li>GENRE</li>
       *   <li>DATE</li>
       *   <li>LOCATION</li>
       *   <li>CONTACT</li>
       *   <li>ISRC</li>
       * </ul>
       *
       * For a more detailed description of these fields, please see the Ogg
       * Vorbis specification, section 5.2.2.1.
       *
       * \note The Ogg Vorbis comment specification does allow these key values
       * to be either upper or lower case.  However, it is conventional for them
       * to be upper case.  As such, TagLib, when parsing a Xiph/Vorbis comment,
       * converts all fields to uppercase.  When you are using this data
       * structure, you will need to specify the field name in upper case.
       *
       * \warning You should not modify this data structure directly, instead
       * use addField() and removeField().
       */
      const FieldListMap &fieldListMap() const;

      /*!
       * Implements the unified property interface -- export function.
       * The result is a one-to-one match of the Xiph comment, since it is
       * completely compatible with the property interface (in fact, a Xiph
       * comment is nothing more than a map from tag names to list of values,
       * as is the dict interface).
       */
      PropertyMap properties() const override;

      /*!
       * Implements the unified property interface -- import function.
       * The tags from the given map will be stored one-to-one in the file,
       * except for invalid keys (less than one character, non-ASCII, or
       * containing '=' or '~') in which case the according values will
       * be contained in the returned PropertyMap.
       */
      PropertyMap setProperties(const PropertyMap&) override;

      StringList complexPropertyKeys() const override;
      List<VariantMap> complexProperties(const String &key) const override;
      bool setComplexProperties(const String &key, const List<VariantMap> &value) override;

      /*!
       * Check if the given String is a valid Xiph comment key.
       */
      static bool checkKey(const String&);

      /*!
       * Returns the vendor ID of the Ogg Vorbis encoder.  libvorbis 1.0 as the
       * most common case always returns "Xiph.Org libVorbis I 20020717".
       */
      String vendorID() const;

      /*!
       * Add the field specified by \a key with the data \a value.  If \a replace
       * is \c true, then all of the other fields with the same key will be removed
       * first.
       *
       * If the field value is empty, the field will be removed.
       */
      void addField(const String &key, const String &value, bool replace = true);

      /*!
       * Remove all the fields specified by \a key.
       *
       * \see removeAllFields()
       */
      void removeFields(const String &key);

      /*!
       * Remove all the fields specified by \a key with the data \a value.
       *
       * \see removeAllFields()
       */
      void removeFields(const String &key, const String &value);

      /*!
       * Remove all the fields in the comment.
       *
       * \see removeFields()
       */
      void removeAllFields();

      /*!
       * Returns \c true if the field is contained within the comment.
       *
       * \note This is safer than checking for membership in the FieldListMap.
       */
      bool contains(const String &key) const;

      /*!
       * Renders the comment to a ByteVector suitable for inserting into a file.
       *
       * If \a addFramingBit is \c true the standard Vorbis comment framing bit will
       * be appended.  However some formats (notably FLAC) do not work with this
       * in place.
       */
      ByteVector render(bool addFramingBit = true) const;


      /*!
       * Returns a list of pictures attached to the xiph comment.
       */
      List<FLAC::Picture *> pictureList();

      /*!
       * Removes a picture. If \a del is \c true the picture's memory
       * will be freed; if it is \c false, it must be deleted by the user.
       */
      void removePicture(FLAC::Picture *picture, bool del = true);

      /*!
       * Remove all pictures.
       */
      void removeAllPictures();

      /*!
       * Add a new picture to the comment block. The comment block takes ownership of the
       * picture and will handle freeing its memory.
       *
       * \note The file will be saved only after calling save().
       */
      void addPicture(FLAC::Picture *picture);

    protected:
      /*!
       * Reads the tag from the file specified in the constructor and fills the
       * FieldListMap.
       */
      void parse(const ByteVector &data);

    private:
      class XiphCommentPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<XiphCommentPrivate> d;
    };
  }  // namespace Ogg
}  // namespace TagLib

#endif
