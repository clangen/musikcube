//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, Casey Langen
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

#pragma once

#include <boost/utility.hpp>    // noncopyable

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// Menu related types
//////////////////////////////////////////////////////////////////////////////

class Menu;
typedef boost::shared_ptr<Menu> MenuRef;

class MenuItem;
typedef boost::shared_ptr<MenuItem> MenuItemRef;

typedef sigslot::signal1<MenuItemRef> MenuItemActivatedEvent;
typedef sigslot::signal2<MenuItemRef /*new*/, unsigned /*index*/> MenuItemAddedEvent;
typedef sigslot::signal1<MenuItemRef> MenuItemRemovedEvent;

class InvalidMenuItemException: public Exception { };
class MenuItemAlreadyExistsException: public Exception { };
class NullMenuItemException: public Exception { };
class IndexOutOfRangeException: public Exception { };

//////////////////////////////////////////////////////////////////////////////
// MenuItem
//////////////////////////////////////////////////////////////////////////////

// unimplemented
class MenuItem: public boost::noncopyable
{
    friend class Menu;  // uses Changed and FillMenuItemInfo:

protected: // constructors
    /*ctor*/    MenuItem(const uistring& caption, MenuRef subMenu = MenuRef());

public: // events
    MenuItemActivatedEvent  Activated;

protected: // events
    sigslot::signal1<MenuItemRef> Changed;

public: // methods
    static MenuItemRef Create(const uistring& caption, MenuRef subMenu = MenuRef());

    void            SetCaption(const uistring& caption);
    uistring        Caption() const;
    void            SetSubMenu(MenuRef subMenu);
    MenuRef         SubMenu() const;

protected: // methods
    virtual void    FillMenuItemInfo(MENUITEMINFO& target);
    virtual void    OnChanged();
    UINT static     NextID();

protected: // instance data
    UINT id;
    boost::weak_ptr<MenuItem> weakThis;

private: // instance data
    MenuRef subMenu;
    uistring caption;
};

//////////////////////////////////////////////////////////////////////////////
// SeparatorMenuItem
//////////////////////////////////////////////////////////////////////////////

class SeparatorMenuItem: public MenuItem
{
private: //types
    typedef MenuItem base;

private: // constructors
    /*ctor*/ SeparatorMenuItem();

public: // methods
    static MenuItemRef Create();

protected: // methods
    virtual void FillMenuItemInfo(MENUITEMINFO& target);
};

//////////////////////////////////////////////////////////////////////////////
// MenuItemCollection
//////////////////////////////////////////////////////////////////////////////

// implemented, untested
class MenuItemCollection: public boost::noncopyable
{
    friend class Menu;

public: // events
    MenuItemAddedEvent      ItemAdded;
    MenuItemRemovedEvent    ItemRemoved;

private: // types
    typedef std::vector<MenuItemRef> MenuItemList;

private: // constructors
    /*ctor*/        MenuItemCollection(Menu& owner);    

public: // methods
    MenuItemRef     Append(MenuItemRef newMenuItem);
    MenuItemRef     InsertAfter(MenuItemRef newMenuItem, MenuItemRef after);
    MenuItemRef     InsertBefore(MenuItemRef newMenuItem, MenuItemRef before);
    void            Remove(MenuItemRef toRemove);
    unsigned        Count();
    MenuItemRef     ItemAt(unsigned index);
    MenuItemRef     operator[](unsigned index);

protected: // methods
    virtual void    OnItemAdded(MenuItemRef newMenuItem, unsigned index);
    virtual void    OnItemRemoved(MenuItemRef oldMenuItem);
    MenuItemRef     InsertWithOffset(MenuItemRef newMenuItem, MenuItemRef insertPoint, unsigned offset);

private: // instance data
    MenuItemList    menuItemList;
    Menu&           owner;
};

//////////////////////////////////////////////////////////////////////////////
// Menu
//////////////////////////////////////////////////////////////////////////////

// Only menu related class that holds any WIN32 handle or state.
class Menu: public boost::noncopyable, public EventHandler
{
    friend class Window;

private: // types
    typedef std::map<UINT, MenuItemRef> IDToMenuItemMap;

public: // constructors, destructors
    /*ctor*/    Menu();
    /*dtor*/    ~Menu();

public: // methods
    MenuItemCollection& Items() { return (*this->items); }
    HMENU   Handle() { return this->menuHandle; }

protected: // methods
    static void     ItemActivated(UINT menuID);   // used by Window
    void            OnItemAdded(MenuItemRef newMenuItem, unsigned index);
    void            OnItemRemoved(MenuItemRef oldMenuItem);
    void            OnItemChanged(MenuItemRef menuItem);

private: // instance data
    boost::scoped_ptr<MenuItemCollection> items;
    HMENU menuHandle;
    static IDToMenuItemMap sIDToMenuItemRef;
};

//////////////////////////////////////////////////////////////////////////////

} // musik::cube::win32cpp