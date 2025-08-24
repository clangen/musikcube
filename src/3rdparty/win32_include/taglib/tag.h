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

#ifndef TAGLIB_TAG_H
#define TAGLIB_TAG_H

#include "taglib_export.h"
#include "tstring.h"
#include "tlist.h"
#include "tvariant.h"

namespace TagLib {

  class PropertyMap;

  //! A simple, generic interface to common audio metadata fields.

  /*!
   * This is an attempt to abstract away the difference in the metadata formats
   * of various audio codecs and tagging schemes.  As such it is generally a
   * subset of what is available in the specific formats but should be suitable
   * for most applications.  This is meant to compliment the generic APIs found
   * in TagLib::AudioProperties, TagLib::File and TagLib::FileRef.
   */

  class TAGLIB_EXPORT Tag
  {
  public:

    /*!
     * Destroys this Tag instance.
     */
    virtual ~Tag();

    Tag(const Tag &) = delete;
    Tag &operator=(const Tag &) = delete;

    /*!
     * Exports the tags of the file as dictionary mapping (human readable) tag
     * names (Strings) to StringLists of tag values.
     * The default implementation in this class considers only the usual built-in
     * tags (artist, album, ...) and only one value per key.
     */
    virtual PropertyMap properties() const;

    /*!
     * Removes unsupported properties, or a subset of them, from the tag.
     * The parameter \a properties must contain only entries from
     * properties().unsupportedData().
     */
    virtual void removeUnsupportedProperties(const StringList& properties);

    /*!
     * Sets the tags of this File to those specified in \a origProps. This default
     * implementation sets only the tags for which setter methods exist in this class
     * (artist, album, ...), and only one value per key; the rest will be contained
     * in the returned PropertyMap.
     */
    virtual PropertyMap setProperties(const PropertyMap &origProps);

    /*!
     * Get the keys of complex properties, i.e. properties which cannot be
     * represented simply by a string.
     * Because such properties might be expensive to fetch, there are separate
     * operations to get the available keys - which is expected to be cheap -
     * and getting and setting the property values.
     * The default implementation returns only an empty list.  Reimplementations
     * should provide "PICTURE" if embedded cover art is present, and optionally
     * support other properties.
     */
    virtual StringList complexPropertyKeys() const;

    /*!
     * Get the complex properties for a given \a key.
     * In order to be flexible for different metadata formats, the properties
     * are represented as variant maps.  Despite this dynamic nature, some
     * degree of standardization should be achieved between formats:
     *
     * - PICTURE
     *   - data: ByteVector with picture data
     *   - description: String with description
     *   - pictureType: String with type as specified for ID3v2,
     *     e.g. "Front Cover", "Back Cover", "Band"
     *   - mimeType: String with image format, e.g. "image/jpeg"
     *   - optionally more information found in the tag, such as
     *     "width", "height", "numColors", "colorDepth" int values
     *     in FLAC pictures
     * - GENERALOBJECT
     *   - data: ByteVector with object data
     *   - description: String with description
     *   - fileName: String with file name
     *   - mimeType: String with MIME type
     *   - this is currently only implemented for ID3v2 GEOB frames
     */
    virtual List<VariantMap> complexProperties(const String &key) const;

    /*!
     * Set all complex properties for a given \a key using variant maps as
     * \a value with the same format as returned by complexProperties().
     * An empty list as \a value removes all complex properties for \a key.
     */
    virtual bool setComplexProperties(const String &key, const List<VariantMap> &value);

    /*!
     * Returns the track name; if no track name is present in the tag
     * an empty string will be returned.
     */
    virtual String title() const = 0;

    /*!
     * Returns the artist name; if no artist name is present in the tag
     * an empty string will be returned.
     */
    virtual String artist() const = 0;

    /*!
     * Returns the album name; if no album name is present in the tag
     * an empty string will be returned.
     */
    virtual String album() const = 0;

    /*!
     * Returns the track comment; if no comment is present in the tag
     * an empty string will be returned.
     */
    virtual String comment() const = 0;

    /*!
     * Returns the genre name; if no genre is present in the tag an empty string
     * will be returned.
     */
    virtual String genre() const = 0;

    /*!
     * Returns the year; if there is no year set, this will return 0.
     */
    virtual unsigned int year() const = 0;

    /*!
     * Returns the track number; if there is no track number set, this will
     * return 0.
     */
    virtual unsigned int track() const = 0;

    /*!
     * Sets the title to \a s.  If \a s is an empty string then this value will be
     * cleared.
     */
    virtual void setTitle(const String &s) = 0;

    /*!
     * Sets the artist to \a s.  If \a s is an empty string then this value will be
     * cleared.
     */
    virtual void setArtist(const String &s) = 0;

    /*!
     * Sets the album to \a s.  If \a s is an empty string then this value will be
     * cleared.
     */
    virtual void setAlbum(const String &s) = 0;

    /*!
     * Sets the comment to \a s.  If \a s is an empty string then this value will be
     * cleared.
     */
    virtual void setComment(const String &s) = 0;

    /*!
     * Sets the genre to \a s.  If \a s is an empty string then this value will be
     * cleared.  For tag formats that use a fixed set of genres, the appropriate
     * value will be selected based on a string comparison.  A list of available
     * genres for those formats should be available in that type's
     * implementation.
     */
    virtual void setGenre(const String &s) = 0;

    /*!
     * Sets the year to \a i.  If \a s is 0 then this value will be cleared.
     */
    virtual void setYear(unsigned int i) = 0;

    /*!
     * Sets the track to \a i.  If \a s is 0 then this value will be cleared.
     */
    virtual void setTrack(unsigned int i) = 0;

    /*!
     * Returns \c true if the tag does not contain any data.  This should be
     * reimplemented in subclasses that provide more than the basic tagging
     * abilities in this class.
     */
    virtual bool isEmpty() const;

    /*!
     * Copies the generic data from one tag to another.
     *
     * \note This will not affect any of the lower level details of the tag.  For
     * instance if any of the tag type specific data (maybe a URL for a band) is
     * set, this will not modify or copy that.  This just copies using the API
     * in this class.
     *
     * If \a overwrite is \c true then the values will be unconditionally copied.
     * If \c false only empty values will be overwritten.
     */
    static void duplicate(const Tag *source, Tag *target, bool overwrite = true);

    /*!
     * Join the \a values of a tag to a single string separated by " / ".
     * If the tag implementation can have multiple values for a basic tag
     * (e.g. artist), they can be combined to a single string for the basic
     * tag getters (e.g. artist()).
     */
    static String joinTagValues(const StringList &values);

  protected:
    /*!
     * Construct a Tag.  This is protected since tags should only be instantiated
     * through subclasses.
     */
    Tag();

  private:
    class TagPrivate;
    TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
    std::unique_ptr<TagPrivate> d;
  };
}  // namespace TagLib

#endif
