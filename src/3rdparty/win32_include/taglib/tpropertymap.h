/***************************************************************************
    copyright           : (C) 2012 by Michael Helmling
    email               : helmling@mathematik.uni-kl.de
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

#ifndef TAGLIB_PROPERTYMAP_H
#define TAGLIB_PROPERTYMAP_H

#include "tmap.h"
#include "tstringlist.h"

#ifdef _MSC_VER
// Explained at end of tpropertymap.cpp
TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
extern template class TagLib::Map<TagLib::String, TagLib::StringList>;
#endif

namespace TagLib {

  using SimplePropertyMap = Map<String, StringList>;

  //! A map for format-independent <key,values> tag representations.

  /*!
   * This map implements a generic representation of textual audio metadata
   * ("tags") realized as pairs of a case-insensitive key
   * and a nonempty list of corresponding values, each value being an arbitrary
   * unicode String.
   *
   * See \ref p_propertymapping for the mapping of the different formats to
   * properties.
   *
   * Note that most metadata formats pose additional conditions on the tag keys. The
   * most popular ones (Vorbis, APE, ID3v2) should support all ASCII only words of
   * length between 2 and 16.
   *
   * This class can contain any tags, but here is a list of "well-known" tags that
   * you might want to use:
   *
   * Basic tags:
   *
   *  - TITLE
   *  - ALBUM
   *  - ARTIST
   *  - ALBUMARTIST
   *  - SUBTITLE
   *  - TRACKNUMBER
   *  - DISCNUMBER
   *  - DATE
   *  - ORIGINALDATE
   *  - GENRE
   *  - COMMENT
   *
   * Sort names:
   *
   *  - TITLESORT
   *  - ALBUMSORT
   *  - ARTISTSORT
   *  - ALBUMARTISTSORT
   *  - COMPOSERSORT
   *
   * Credits:
   *
   *  - COMPOSER
   *  - LYRICIST
   *  - CONDUCTOR
   *  - REMIXER
   *  - PERFORMER:\<XXXX>
   *
   * Other tags:
   *
   *  - ISRC
   *  - ASIN
   *  - BPM
   *  - COPYRIGHT
   *  - ENCODEDBY
   *  - MOOD
   *  - COMMENT
   *  - MEDIA
   *  - LABEL
   *  - CATALOGNUMBER
   *  - BARCODE
   *  - RELEASECOUNTRY
   *  - RELEASESTATUS
   *  - RELEASETYPE
   *
   * MusicBrainz identifiers:
   *
   *  - MUSICBRAINZ_TRACKID
   *  - MUSICBRAINZ_ALBUMID
   *  - MUSICBRAINZ_RELEASEGROUPID
   *  - MUSICBRAINZ_RELEASETRACKID
   *  - MUSICBRAINZ_WORKID
   *  - MUSICBRAINZ_ARTISTID
   *  - MUSICBRAINZ_ALBUMARTISTID
   *  - ACOUSTID_ID
   *  - ACOUSTID_FINGERPRINT
   *  - MUSICIP_PUID
   *
   */

  class PropertyMap: public SimplePropertyMap
  {
  public:
    using Iterator = SimplePropertyMap::Iterator;
    using ConstIterator = SimplePropertyMap::ConstIterator;

    TAGLIB_EXPORT
    PropertyMap();

    TAGLIB_EXPORT
    PropertyMap(const PropertyMap &m);

    TAGLIB_EXPORT
    PropertyMap &operator=(const PropertyMap &other);

    /*!
     * Creates a PropertyMap initialized from a SimplePropertyMap. Copies all
     * entries from \a m that have valid keys.
     * Invalid keys will be appended to the unsupportedData() list.
     */
    TAGLIB_EXPORT
    PropertyMap(const SimplePropertyMap &m);

    TAGLIB_EXPORT
    ~PropertyMap();

    /*!
     * Inserts \a values under \a key in the map.  If \a key already exists,
     * then \a values will be appended to the existing StringList.
     * The returned value indicates success, i.e. whether \a key is a
     * valid key.
     */
    TAGLIB_EXPORT
    bool insert(const String &key, const StringList &values);

    /*!
     * Replaces any existing values for \a key with the given \a values,
     * and simply insert them if \a key did not exist before.
     * The returned value indicates success, i.e. whether \a key is a
     * valid key.
     */
    TAGLIB_EXPORT
    bool replace(const String &key, const StringList &values);

    /*!
     * Find the first occurrence of \a key.
     */
    TAGLIB_EXPORT
    Iterator find(const String &key);

    /*!
     * Find the first occurrence of \a key.
     */
    TAGLIB_EXPORT
    ConstIterator find(const String &key) const;

    /*!
     * Returns \c true if the map contains values for \a key.
     */
    TAGLIB_EXPORT
    bool contains(const String &key) const;

    /*!
     * Returns \c true if this map contains all keys of \a other
     * and the values coincide for those keys. Does not take
     * the unsupportedData list into account.
     */
    TAGLIB_EXPORT
    bool contains(const PropertyMap &other) const;

    /*!
     * Erase the \a key and its values from the map.
     */
    TAGLIB_EXPORT
    PropertyMap &erase(const String &key);

    /*!
     * Erases from this map all keys that appear in \a other.
     */
    TAGLIB_EXPORT
    PropertyMap &erase(const PropertyMap &other);

    /*!
     * Merge the contents of \a other into this PropertyMap.
     * If a key is contained in both maps, the values of the second
     * are appended to that of the first.
     * The unsupportedData() lists are concatenated as well.
     */
    TAGLIB_EXPORT
    PropertyMap &merge(const PropertyMap &other);

    /*!
     * Returns the value associated with \a key.
     *
     * If the map does not contain \a key, it returns \a defaultValue.
     * If no \a defaultValue is specified, it returns an empty string list.
     */
    TAGLIB_EXPORT
    StringList value(const String &key,
                     const StringList &defaultValue = StringList()) const;

    /*!
     * Returns a reference to the value associated with \a key.
     *
     * \note If \a key is not contained in the map, an empty
     * StringList is returned without error.
     */
    TAGLIB_EXPORT
    const StringList &operator[](const String &key) const;

    /*!
     * Returns a reference to the value associated with \a key.
     *
     * \note If \a key is not contained in the map, an empty
     * StringList is returned. You can also directly add entries
     * by using this function as an lvalue.
     */
    TAGLIB_EXPORT
    StringList &operator[](const String &key);

    /*!
     * Returns \c true if and only if \a other has the same contents as this map.
     */
    TAGLIB_EXPORT
    bool operator==(const PropertyMap &other) const;

    /*!
     * Returns \c false if and only if \a other has the same contents as this map.
     */
    TAGLIB_EXPORT
    bool operator!=(const PropertyMap &other) const;

    /*!
     * If a PropertyMap is read from a File object using File::properties()
     * (or a FileRef object using FileRef::properties()),
     * the StringList returned from this function will represent metadata
     * that could not be parsed into the PropertyMap representation. This could
     * be e.g. binary data, unknown ID3 frames, etc.
     *
     * \see File::removeUnsupportedProperties(),
     *      FileRef::removeUnsupportedProperties()
     */
    TAGLIB_EXPORT
    const StringList &unsupportedData() const;

    /*!
     * Add property \a key to list of unsupported data.
     *
     * \see unsupportedData()
     */
    TAGLIB_EXPORT
    void addUnsupportedData(const String &key);

    /*!
     * Removes all entries which have an empty value list.
     */
    TAGLIB_EXPORT
    void removeEmpty();

    TAGLIB_EXPORT
    String toString() const;

  private:
    class PropertyMapPrivate;
    std::unique_ptr<PropertyMapPrivate> d;
  };

}  // namespace TagLib
#endif /* TAGLIB_PROPERTYMAP_H */
