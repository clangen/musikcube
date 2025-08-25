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

#ifndef TAGLIB_STRINGLIST_H
#define TAGLIB_STRINGLIST_H

#include "tstring.h"
#include "tlist.h"
#include "tbytevectorlist.h"
#include "taglib_export.h"

namespace TagLib {

  //! A list of strings

  /*!
   * This is a specialization of the List class with some convenience members
   * for string operations.
   */

  class StringList : public List<String>
  {
  public:

    /*!
     * Constructs an empty StringList.
     */
    TAGLIB_EXPORT
    StringList();

    /*!
     * Make a shallow, implicitly shared, copy of \a l.  Because this is
     * implicitly shared, this method is lightweight and suitable for
     * pass-by-value usage.
     */
    TAGLIB_EXPORT
    StringList(const StringList &l);

    /*!
     * Construct a StringList with the contents of the braced initializer list.
     */
    TAGLIB_EXPORT
    StringList(std::initializer_list<String> init);

    TAGLIB_EXPORT
    StringList &operator=(const StringList &);
    TAGLIB_EXPORT
    StringList &operator=(std::initializer_list<String> init);

    /*!
     * Constructs a StringList with \a s as a member.
     */
    TAGLIB_EXPORT
    StringList(const String &s);

    /*!
     * Makes a deep copy of the data in \a bl.
     *
     * \note This should only be used with the 8-bit codecs Latin1 and UTF8, when
     * used with other codecs it will simply print a warning and exit.
     */
    TAGLIB_EXPORT
    StringList(const ByteVectorList &bl, String::Type t = String::Latin1);

    /*!
     * Destroys this StringList instance.
     */
    TAGLIB_EXPORT
    ~StringList();

    /*!
     * Concatenate the list of strings into one string separated by \a separator.
     */
    TAGLIB_EXPORT
    String toString(const String &separator = " ") const;

    /*!
     * Appends \a s to the end of the list and returns a reference to the
     * list.
     */
    TAGLIB_EXPORT
    StringList &append(const String &s);

    /*!
     * Appends all of the values in \a l to the end of the list and returns a
     * reference to the list.
     */
    TAGLIB_EXPORT
    StringList &append(const StringList &l);

    /*!
     * Splits the String \a s into several strings at \a pattern.  This will not include
     * the pattern in the returned strings.
     */
    TAGLIB_EXPORT
    static StringList split(const String &s, const String &pattern);

  private:
    class StringListPrivate;
    std::unique_ptr<StringListPrivate> d;
  };

}  // namespace TagLib

/*!
 * \related TagLib::StringList
 * Send the StringList to an output stream.
 */
std::ostream TAGLIB_EXPORT &operator<<(std::ostream &s, const TagLib::StringList &l);

#endif
