/**************************************************************************
    copyright            : (C) 2007 by Lukáš Lalinský
    email                : lalinsky@gmail.com
 **************************************************************************/

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

#ifndef TAGLIB_MP4ITEM_H
#define TAGLIB_MP4ITEM_H

#include "tstringlist.h"
#include "taglib_export.h"
#include "mp4coverart.h"

namespace TagLib {
  namespace MP4 {
    //! MP4 item
    class TAGLIB_EXPORT Item
    {
    public:
      /*!
       * The data type stored in the item.
       */
      enum class Type : unsigned char {
        Void,
        Bool,
        Int,
        IntPair,
        Byte,
        UInt,
        LongLong,
        StringList,
        ByteVectorList,
        CoverArtList
      };

      struct IntPair {
        int first, second;
      };

      Item();
      Item(const Item &item);

      /*!
       * Copies the contents of \a item into this Item.
       */
      Item &operator=(const Item &item);

      /*!
       * Exchanges the content of the Item with the content of \a item.
       */
      void swap(Item &item) noexcept;

      ~Item();

      Item(int value);
      Item(unsigned char value);
      Item(unsigned int value);
      Item(long long value);
      Item(bool value);
      Item(int value1, int value2);
      Item(const StringList &value);
      Item(const ByteVectorList &value);
      Item(const CoverArtList &value);

      void setAtomDataType(AtomDataType type);
      AtomDataType atomDataType() const;

      int toInt() const;
      unsigned char toByte() const;
      unsigned int toUInt() const;
      long long toLongLong() const;
      bool toBool() const;
      IntPair toIntPair() const;
      StringList toStringList() const;
      ByteVectorList toByteVectorList() const;
      CoverArtList toCoverArtList() const;

      bool isValid() const;

      Type type() const;

      /*!
       * Returns \c true if the Item and \a other are of the same type and
       * contain the same value.
       */
      bool operator==(const Item &other) const;

      /*!
       * Returns \c true if the Item and \a other  differ in type or value.
       */
      bool operator!=(const Item &other) const;

    private:
      class ItemPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::shared_ptr<ItemPrivate> d;
    };

    using ItemMap = TagLib::Map<String, Item>;
  }  // namespace MP4
}  // namespace TagLib
#endif
