//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2007, Casey Langen
//
// Sources and Binaries of: win32cpp
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

/**
 * @file Menu.hpp
 * @brief Menu bar and popup menu support.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. This header
 * defines Menu (the only menu class that holds a Win32 HMENU), MenuItem,
 * SeparatorMenuItem and MenuItemCollection. Items are shared via MenuItemRef
 * smart pointers and notify through the sigslot observer pattern.
 */

#pragma once

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Exception.hpp>
#include <win32cpp/Types.hpp>

#include <vector>
#include <map>
#include <boost/utility.hpp>    // noncopyable
#include <sigslot/sigslot.h>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// Menu related types
//////////////////////////////////////////////////////////////////////////////

class Menu;
typedef std::shared_ptr<Menu> MenuRef;   /**< shared pointer to a Menu */

class MenuItem;
typedef std::shared_ptr<MenuItem> MenuItemRef; /**< shared pointer to a MenuItem */

/** @brief Signal emitted when a menu item is activated. */
typedef sigslot::signal1<MenuItemRef> MenuItemActivatedEvent;
/** @brief Signal emitted when a menu item is added (new item, index). */
typedef sigslot::signal2<MenuItemRef /*new*/, unsigned /*index*/> MenuItemAddedEvent;
/** @brief Signal emitted when a menu item is removed. */
typedef sigslot::signal1<MenuItemRef> MenuItemRemovedEvent;

class InvalidMenuItemException: public Exception { };
class MenuItemAlreadyExistsException: public Exception { };
class NullMenuItemException: public Exception { };
class IndexOutOfRangeException: public Exception { };

//////////////////////////////////////////////////////////////////////////////
// MenuItem
//////////////////////////////////////////////////////////////////////////////

/** @brief A single entry in a menu.
 *  @details Items are shared through MenuItemRef. When activated, the
 *           Activated signal is emitted. Items may have an associated
 *           sub-menu and a check state. */
class MenuItem: public boost::noncopyable
{
    friend class Menu;  // uses Changed and FillMenuItemInfo:

protected: // constructors
    /** @brief Constructs a menu item.
     *  @param caption the item's display text
     *  @param subMenu optional sub-menu to attach */
    /*ctor*/    MenuItem(const uistring& caption, MenuRef subMenu = MenuRef());

public: // events
    /** @brief Emitted when the item is activated by the user. */
    MenuItemActivatedEvent  Activated;

protected: // events
    sigslot::signal1<MenuItemRef> Changed; /**< emitted when item attributes change */

public: // methods
    /** @brief Creates a menu item.
     *  @param caption the item's display text
     *  @param subMenu optional sub-menu to attach
     *  @return a shared pointer to the new item */
    static MenuItemRef Create(const uistring& caption, MenuRef subMenu = MenuRef());

    /** @brief Sets the item's display text.
     *  @param caption the new caption */
    void            SetCaption(const uistring& caption);
    /** @brief Returns the item's display text.
     *  @return the caption string */
    uistring        Caption() const;
    /** @brief Attaches a sub-menu to the item.
     *  @param subMenu the sub-menu to attach */
    void            SetSubMenu(MenuRef subMenu);
    /** @brief Returns the item's sub-menu.
     *  @return the sub-menu, or NULL */
    MenuRef         SubMenu() const;
    /** @brief Returns whether the item is checked.
     *  @return true if checked */
    bool            Checked() const { return this->checked; }
    /** @brief Sets the item's check state.
     *  @param checked true to check the item */
    void            SetChecked(bool  checked);

protected: // methods
    /** @brief Fills a MENUITEMINFO for the Win32 menu API.
     *  @param target the MENUITEMINFO to fill */
    virtual void    FillMenuItemInfo(MENUITEMINFO& target);
    /** @brief Emits the Changed signal. */
    virtual void    OnChanged();
    /** @brief Allocates a unique menu item ID.
     *  @return the next available ID */
    UINT static     NextID();

protected: // instance data
    UINT id;                       /**< unique menu item ID */
    std::weak_ptr<MenuItem> weakThis; /**< weak self-reference */

private: // instance data
    MenuRef subMenu;   /**< attached sub-menu */
    uistring caption;  /**< display text */
    bool checked;      /**< check state */
};

//////////////////////////////////////////////////////////////////////////////
// SeparatorMenuItem
//////////////////////////////////////////////////////////////////////////////

/** @brief A menu separator (dividing) line. */
class SeparatorMenuItem: public MenuItem
{
private: //types
    typedef MenuItem base;

private: // constructors
    /** @brief Constructs a separator item. */
    /*ctor*/ SeparatorMenuItem();

public: // methods
    /** @brief Creates a separator item.
     *  @return a shared pointer to the new separator */
    static MenuItemRef Create();

protected: // methods
    /** @brief Fills a MENUITEMINFO with separator data.
     *  @param target the MENUITEMINFO to fill */
    virtual void FillMenuItemInfo(MENUITEMINFO& target);
};

//////////////////////////////////////////////////////////////////////////////
// MenuItemCollection
//////////////////////////////////////////////////////////////////////////////

/** @brief An ordered collection of menu items owned by a Menu.
 *  @details Provides append/insert/remove operations on the item list and
 *           notifies through the ItemAdded and ItemRemoved signals. */
class MenuItemCollection: public boost::noncopyable
{
    friend class Menu;

public: // events
    /** @brief Emitted when an item is added. */
    MenuItemAddedEvent      ItemAdded;
    /** @brief Emitted when an item is removed. */
    MenuItemRemovedEvent    ItemRemoved;

private: // types
    typedef std::vector<MenuItemRef> MenuItemList;

private: // constructors
    /** @brief Constructs the collection for the given owner menu.
     *  @param owner the owning Menu */
    /*ctor*/        MenuItemCollection(Menu& owner);

public: // methods
    /** @brief Appends an item to the collection.
     *  @param newMenuItem the item to add
     *  @return the added item */
    MenuItemRef     Append(MenuItemRef newMenuItem);
    /** @brief Inserts an item after another item.
     *  @param newMenuItem the item to insert
     *  @param after the item to insert after
     *  @return the inserted item */
    MenuItemRef     InsertAfter(MenuItemRef newMenuItem, MenuItemRef after);
    /** @brief Inserts an item before another item.
     *  @param newMenuItem the item to insert
     *  @param before the item to insert before
     *  @return the inserted item */
    MenuItemRef     InsertBefore(MenuItemRef newMenuItem, MenuItemRef before);
    /** @brief Removes an item from the collection.
     *  @param toRemove the item to remove */
    void            Remove(MenuItemRef toRemove);
    /** @brief Returns the number of items.
     *  @return the item count */
    unsigned        Count();
    /** @brief Returns the item at the given index.
     *  @param index the item index
     *  @return the item, or NULL */
    MenuItemRef     ItemAt(unsigned index);
    /** @brief Finds an item by caption.
     *  @param name the caption to search for
     *  @param offset the index to start searching from
     *  @return the matching item, or NULL */
    MenuItemRef     FindByCaption(const uistring& name, unsigned offset = 0);
    /** @brief Returns the item at the given index.
     *  @param index the item index
     *  @return the item, or NULL */
    MenuItemRef     operator[](unsigned index);

protected: // methods
    /** @brief Emits ItemAdded and updates the owner menu.
     *  @param newMenuItem the added item
     *  @param index the insertion index */
    virtual void    OnItemAdded(MenuItemRef newMenuItem, unsigned index);
    /** @brief Emits ItemRemoved and updates the owner menu.
     *  @param oldMenuItem the removed item */
    virtual void    OnItemRemoved(MenuItemRef oldMenuItem);
    /** @brief Inserts an item with an explicit offset.
     *  @param newMenuItem the item to insert
     *  @param insertPoint the reference item
     *  @param offset the insertion offset
     *  @return the inserted item */
    MenuItemRef     InsertWithOffset(MenuItemRef newMenuItem, MenuItemRef insertPoint, unsigned offset);

private: // instance data
    MenuItemList    menuItemList; /**< the ordered item list */
    Menu&           owner;        /**< owning Menu */
};

//////////////////////////////////////////////////////////////////////////////
// Menu
//////////////////////////////////////////////////////////////////////////////

/** @brief A menu bar or popup menu.
 *  @details The only menu-related class that holds any Win32 handle or
 *           state (an HMENU). Build the menu by appending MenuItems to
 *           Items(); popup menus are shown on demand, while menu-bar menus
 *           are attached to a window.
 *  @note Menus are created via Menu::Create() or Menu::CreatePopup(). */
class Menu: public boost::noncopyable, public EventHandler
{
    friend class Window;

private: // types
    typedef std::map<UINT, MenuItemRef> IDToMenuItemMap; /**< maps IDs to items */

public: // constructors, destructors
    /** @brief Destroys the menu, releasing the HMENU. */
    /*dtor*/    ~Menu();

private: // constructors
    /** @brief Constructs an empty menu (use Create() instead). */
    /*ctor*/    Menu();

public: // methods
    /** @brief Creates a menu-bar style menu.
     *  @return a shared pointer to the new menu */
    static MenuRef Create();
    /** @brief Creates a popup menu.
     *  @return a shared pointer to the new popup menu */
    static MenuRef CreatePopup();

    /** @brief Returns the collection of items in this menu.
     *  @return reference to the item collection */
    MenuItemCollection& Items() { return (*this->items); }
    /** @brief Returns the underlying Win32 HMENU.
     *  @return the menu handle */
    HMENU   Handle() { return this->menuHandle; }
    /** @brief Handles activation of the item with the given ID.
     *  @param menuID the activated item's ID
     *  @note Called internally by Window. */
    static void     ItemActivated(UINT menuID);   // used by Window

protected: // methods
    /** @brief Associates this Menu with an existing HMENU.
     *  @param menu the HMENU to take ownership of */
    void            Initialize(HMENU menu);
    /** @brief Updates the Win32 menu when an item is added.
     *  @param newMenuItem the added item
     *  @param index the insertion index */
    void            OnItemAdded(MenuItemRef newMenuItem, unsigned index);
    /** @brief Updates the Win32 menu when an item is removed.
     *  @param oldMenuItem the removed item */
    void            OnItemRemoved(MenuItemRef oldMenuItem);
    /** @brief Updates the Win32 menu when an item changes.
     *  @param menuItem the changed item */
    void            OnItemChanged(MenuItemRef menuItem);

private: // instance data
    std::unique_ptr<MenuItemCollection> items; /**< the item collection */
    HMENU menuHandle;                          /**< the Win32 menu handle */
    static IDToMenuItemMap sIDToMenuItemRef;   /**< global ID-to-item map */
};

//////////////////////////////////////////////////////////////////////////////

} // win32cpp
