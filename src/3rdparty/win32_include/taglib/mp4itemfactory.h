/***************************************************************************
    copyright            : (C) 2023 by Urs Fleisch
    email                : ufleisch@users.sourceforge.net
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

#ifndef TAGLIB_MP4ITEMFACTORY_H
#define TAGLIB_MP4ITEMFACTORY_H

#include <memory>
#include "taglib_export.h"
#include "mp4item.h"

namespace TagLib {

  namespace MP4 {

    //! A factory for creating MP4 items during parsing

    /*!
     * This factory abstracts away the parsing and rendering between atom data
     * and MP4 items.
     *
     * Reimplementing this factory is the key to adding support for atom types
     * not directly supported by TagLib to your application.  To do so you would
     * subclass this factory and reimplement nameHandlerMap() to add support
     * for new atoms.  If the new atoms do not have the same behavior as
     * other supported atoms, it may be necessary to reimplement parseItem() and
     * renderItem().  Then by setting your factory in the MP4::Tag constructor
     * you can implement behavior that will allow for new atom types to be used.
     *
     * A custom item factory adding support for a "tsti" integer atom and a
     * "tstt" text atom can be implemented like this:
     *
     * \code
     * class CustomItemFactory : public MP4::ItemFactory {
     * protected:
     *   NameHandlerMap nameHandlerMap() const override
     *   {
     *     return MP4::ItemFactory::nameHandlerMap()
     *       .insert("tsti", ItemHandlerType::Int)
     *       .insert("tstt", ItemHandlerType::Text);
     *   }
     * };
     * \endcode
     *
     * If the custom item shall also be accessible via a property,
     * namePropertyMap() can be overridden in the same way.
     */
    class TAGLIB_EXPORT ItemFactory
    {
    public:
      ItemFactory(const ItemFactory &) = delete;
      ItemFactory &operator=(const ItemFactory &) = delete;

      static ItemFactory *instance();

      /*!
       * Create an MP4 item from the \a data bytes of an \a atom.
       * Returns the name (in most cases atom->name) and an item,
       * an invalid item on failure.
       * The default implementation uses the map returned by nameHandlerMap().
       */
      virtual std::pair<String, Item> parseItem(
        const Atom *atom, const ByteVector &data) const;

      /*!
       * Render an MP4 \a item to the data bytes of an atom \a itemName.
       * An empty byte vector is returned if the item is invalid or unknown.
       * The default implementation uses the map returned by nameHandlerMap().
       */
      virtual ByteVector renderItem(
        const String &itemName, const Item &item) const;

      /*!
       * Create an MP4 item from a property with \a key and \a values.
       * If the property is not supported, an invalid item is returned.
       * The default implementation uses the map returned by namePropertyMap().
       */
      virtual std::pair<ByteVector, Item> itemFromProperty(
        const String &key, const StringList &values) const;

      /*!
       * Get an MP4 item as a property.
       * If no property exists for \a itemName, an empty string is returned as
       * the property key.
       * The default implementation uses the map returned by namePropertyMap().
       */
      virtual std::pair<String, StringList> itemToProperty(
        const ByteVector &itemName, const Item &item) const;

      /*!
       * Returns property key for atom \a name, empty if no property exists for
       * this atom.
       * The default method looks up the map created by namePropertyMap() and
       * should be enough for most uses.
       */
      virtual String propertyKeyForName(const ByteVector &name) const;

      /*!
       * Returns atom name for property \a key, empty if no property is
       * supported for this key.
       * The default method uses the reverse mapping of propertyKeyForName()
       * and should be enough for most uses.
       */
      virtual ByteVector nameForPropertyKey(const String &key) const;

    protected:
      /*!
       * Type that determines the parsing and rendering between the data and
       * the item representation of an atom.
       */
      enum class ItemHandlerType {
        Unknown,
        FreeForm,
        IntPair,
        IntPairNoTrailing,
        Bool,
        Int,
        TextOrInt,
        UInt,
        LongLong,
        Byte,
        Gnre,
        Covr,
        TextImplicit,
        Text
      };

      /*! Mapping of atom name to handler type. */
      using NameHandlerMap = Map<ByteVector, ItemHandlerType>;

      /*!
       * Constructs an item factory.  Because this is a singleton this method is
       * protected, but may be used for subclasses.
       */
      ItemFactory();

      /*!
       * Destroys the frame factory.
       */
      ~ItemFactory();

      /*!
       * Returns mapping between atom names and handler types.
       * This method is called once by handlerTypeForName() to initialize its
       * internal cache.
       * To add support for a new atom, it is sufficient in most cases to just
       * reimplement this method and add new entries.
       */
      virtual NameHandlerMap nameHandlerMap() const;

      /*!
       * Returns handler type for atom \a name.
       * The default method looks up the map created by nameHandlerMap() and
       * should be enough for most uses.
       */
      virtual ItemHandlerType handlerTypeForName(const ByteVector &name) const;

      /*!
       * Returns mapping between atom names and property keys.
       * This method is called once by propertyKeyForName() to initialize its
       * internal cache.
       * To add support for a new atom with a property, it is sufficient in most
       * cases to just reimplement this method and add new entries.
       */
      virtual Map<ByteVector, String> namePropertyMap() const;

      // Functions used by parseItem() to create items from atom data.
      static MP4::AtomDataList parseData2(
        const MP4::Atom *atom, const ByteVector &data, int expectedFlags = -1,
        bool freeForm = false);
      static ByteVectorList parseData(
        const MP4::Atom *atom, const ByteVector &bytes, int expectedFlags = -1,
        bool freeForm = false);
      static std::pair<String, Item> parseText(
        const MP4::Atom *atom, const ByteVector &bytes, int expectedFlags = 1);
      static std::pair<String, Item> parseFreeForm(
        const MP4::Atom *atom, const ByteVector &bytes);
      static std::pair<String, Item> parseInt(
        const MP4::Atom *atom, const ByteVector &bytes);
      static std::pair<String, Item> parseByte(
        const MP4::Atom *atom, const ByteVector &bytes);
      static std::pair<String, Item> parseTextOrInt(
        const MP4::Atom *atom, const ByteVector &bytes);
      static std::pair<String, Item> parseUInt(
        const MP4::Atom *atom, const ByteVector &bytes);
      static std::pair<String, Item> parseLongLong(
        const MP4::Atom *atom, const ByteVector &bytes);
      static std::pair<String, Item> parseGnre(
        const MP4::Atom *atom, const ByteVector &bytes);
      static std::pair<String, Item> parseIntPair(
        const MP4::Atom *atom, const ByteVector &bytes);
      static std::pair<String, Item> parseBool(
        const MP4::Atom *atom, const ByteVector &bytes);
      static std::pair<String, Item> parseCovr(
        const MP4::Atom *atom, const ByteVector &data);

      // Functions used by renderItem() to render atom data for items.
      static ByteVector renderAtom(
        const ByteVector &name, const ByteVector &data);
      static ByteVector renderData(
        const ByteVector &name, int flags, const ByteVectorList &data);
      static ByteVector renderText(
        const ByteVector &name, const MP4::Item &item, int flags = TypeUTF8);
      static ByteVector renderFreeForm(
        const String &name, const MP4::Item &item);
      static ByteVector renderBool(
        const ByteVector &name, const MP4::Item &item);
      static ByteVector renderInt(
        const ByteVector &name, const MP4::Item &item);
      static ByteVector renderTextOrInt(
        const ByteVector &name, const MP4::Item &item);
      static ByteVector renderByte(
        const ByteVector &name, const MP4::Item &item);
      static ByteVector renderUInt(
        const ByteVector &name, const MP4::Item &item);
      static ByteVector renderLongLong(
        const ByteVector &name, const MP4::Item &item);
      static ByteVector renderIntPair(
        const ByteVector &name, const MP4::Item &item);
      static ByteVector renderIntPairNoTrailing(
        const ByteVector &name, const MP4::Item &item);
      static ByteVector renderCovr(
        const ByteVector &name, const MP4::Item &item);

    private:
      static ItemFactory factory;

      class ItemFactoryPrivate;
      TAGLIB_MSVC_SUPPRESS_WARNING_NEEDS_TO_HAVE_DLL_INTERFACE
      std::unique_ptr<ItemFactoryPrivate> d;
    };

  }  // namespace MP4
}  // namespace TagLib

#endif
