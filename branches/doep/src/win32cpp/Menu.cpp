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

#include <pch.hpp>
#include <win32cpp/Menu.hpp>
#include <win32cpp/Win32Exception.hpp>

#include <boost/scoped_array.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////
// Menu
//////////////////////////////////////////////////////////////////////////////

Menu::IDToMenuItemMap Menu::sIDToMenuItemRef;

/*ctor*/    Menu::Menu()
: menuHandle(NULL)
{
    this->items.reset(new MenuItemCollection(*this));
    this->menuHandle = ::CreateMenu();

    if ( ! this->menuHandle)
    {
        throw Win32Exception();
    }

    this->items->ItemAdded.connect(this, &Menu::OnItemAdded);
    this->items->ItemRemoved.connect(this, &Menu::OnItemRemoved);
}

/*dtor*/    Menu::~Menu()
{
    ::DestroyMenu(this->menuHandle);
}

void        Menu::OnItemAdded(MenuItemRef newMenuItem, unsigned index)
{
    newMenuItem->Changed.connect(this, &Menu::OnItemChanged);

    // used by MenuItemActivated()
    Menu::sIDToMenuItemRef[newMenuItem->id] = newMenuItem;

    if (this->menuHandle)
    {
        MENUITEMINFO menuItem;
        newMenuItem->FillMenuItemInfo(menuItem);

        if ( ! ::InsertMenuItem(this->menuHandle, index, TRUE, &menuItem))
        {
            throw Win32Exception();
        }
    }
}

void        Menu::OnItemRemoved(MenuItemRef oldMenuItem)
{
    // used by MenuItemActivated()
    Menu::sIDToMenuItemRef.erase(oldMenuItem->id);

    if (this->menuHandle)
    {
        if ( ! ::RemoveMenu(this->menuHandle, oldMenuItem->id, MF_BYCOMMAND))
        {
            throw Win32Exception();
        }
    }

    oldMenuItem->Changed.disconnect(this);
}

void        Menu::OnItemChanged(MenuItemRef menuItem)
{
    if (this->menuHandle)
    {
        MENUITEMINFO menuItemInfo;
        menuItem->FillMenuItemInfo(menuItemInfo);

        if ( ! ::SetMenuItemInfo(
            this->menuHandle,
            menuItem->id,
            FALSE,
            &menuItemInfo))
        {
            throw Win32Exception();
        }
    }
}

void        Menu::ItemActivated(UINT menuItemID)
{
    IDToMenuItemMap::iterator it = Menu::sIDToMenuItemRef.find(menuItemID);
    if (it != Menu::sIDToMenuItemRef.end())
    {
        it->second->Activated(it->second);
    }
}

//////////////////////////////////////////////////////////////////////////////
// MenuItem
//////////////////////////////////////////////////////////////////////////////

/*ctor*/            MenuItem::MenuItem(const uistring& caption, MenuRef subMenu)
: caption(caption)
, subMenu(subMenu)
, id(MenuItem::NextID())
{
}

MenuItemRef         MenuItem::Create(const uistring& caption, MenuRef subMenu)
{
    MenuItemRef result = MenuItemRef(new MenuItem(caption, subMenu));
    result->weakThis = boost::weak_ptr<MenuItem>(result);

    return result;
}


void                MenuItem::SetCaption(const uistring& caption)
{
    if (this->caption != caption)
    {
        this->caption = caption;
        this->OnChanged();
    }
}

uistring            MenuItem::Caption() const
{
    return this->caption;
}

void                MenuItem::SetSubMenu(MenuRef subMenu)
{
    if (this->subMenu != subMenu)
    {
        this->subMenu = subMenu;
        this->OnChanged();
    }
}

MenuRef             MenuItem::SubMenu() const
{
    return this->subMenu;
}

UINT                MenuItem::NextID()
{
    static UINT sNextID = 0;
    return sNextID++;
}

void                MenuItem::FillMenuItemInfo(MENUITEMINFO& target)
{
    static uichar caption[4096];
     const size_t captionSize = min(4096, this->caption.size() + 1);
    ::wcsncpy_s(caption, captionSize, this->caption.c_str(), captionSize);
    caption[captionSize - 1] = 0;

    ::SecureZeroMemory(&target, sizeof(MENUITEMINFO));
    target.cbSize = sizeof(MENUITEMINFO);
    target.wID = this->id;
    target.cch = (UINT) (this->caption.size() + 1);
    target.dwTypeData = caption;
    target.hSubMenu = (this->subMenu ? this->subMenu->Handle() : NULL);
    target.fMask = MIIM_STRING | MIIM_ID | MIIM_SUBMENU;
}

void                MenuItem::OnChanged()
{
    this->Changed(MenuItemRef(this->weakThis));
}

//////////////////////////////////////////////////////////////////////////////
// SeparatorMenuItem
//////////////////////////////////////////////////////////////////////////////

MenuItemRef SeparatorMenuItem::Create()
{
    typedef boost::shared_ptr<SeparatorMenuItem> SeparatorMenuItemRef;
    SeparatorMenuItemRef result = SeparatorMenuItemRef(new SeparatorMenuItem());
    result->weakThis = boost::weak_ptr<MenuItem>(result);

    return result;
}

/*ctor*/    SeparatorMenuItem::SeparatorMenuItem()
: base(_T(""))
{
}

void        SeparatorMenuItem::FillMenuItemInfo(MENUITEMINFO& target)
{
    ::SecureZeroMemory(&target, sizeof(MENUITEMINFO));
    target.cbSize = sizeof(MENUITEMINFO);
    target.wID = this->id;
    target.fMask = MIIM_TYPE;
    target.fType = MFT_SEPARATOR;
}


//////////////////////////////////////////////////////////////////////////////
// MenuItemCollection
//////////////////////////////////////////////////////////////////////////////

#if defined(_DEBUG)
#define THROW_IF_MENUITEM_ALREADY_EXISTS(list, menuItem)                \
    if (std::find(list.begin(), list.end(), menuItem) != list.end())    \
    {                                                                   \
        throw MenuItemAlreadyExistsException();                         \
    }
#endif

/*ctor*/    MenuItemCollection::MenuItemCollection(Menu& owner)
: owner(owner)
{
}

MenuItemRef MenuItemCollection::Append(MenuItemRef newMenuItem)
{
#if defined(_DEBUG)
    THROW_IF_MENUITEM_ALREADY_EXISTS(this->menuItemList, newMenuItem);
#endif

    this->menuItemList.push_back(newMenuItem);

    unsigned index = (unsigned) (this->menuItemList.size() - 1);
    this->OnItemAdded(newMenuItem, index);

    return newMenuItem;
}

MenuItemRef MenuItemCollection::InsertWithOffset(MenuItemRef newMenuItem, MenuItemRef insertPoint, unsigned offset)
{
    for (size_t i = 0; i < this->menuItemList.size(); i++)
    {
        if (insertPoint == this->menuItemList[i])
        {
            MenuItemList::iterator it = this->menuItemList.begin() + i + offset;

            this->menuItemList.insert(it, newMenuItem);
            this->OnItemAdded(newMenuItem, (unsigned) (i + offset));

            return newMenuItem;
        }
    }

    throw InvalidMenuItemException();
}

MenuItemRef MenuItemCollection::InsertAfter(MenuItemRef newMenuItem, MenuItemRef after)
{
#if defined(_DEBUG)
    THROW_IF_MENUITEM_ALREADY_EXISTS(this->menuItemList, newMenuItem);
#endif

    return this->InsertWithOffset(newMenuItem, after, 1);
}

MenuItemRef MenuItemCollection::InsertBefore(MenuItemRef newMenuItem, MenuItemRef before)
{
#if defined(_DEBUG)
    THROW_IF_MENUITEM_ALREADY_EXISTS(this->menuItemList, newMenuItem);
#endif

    return this->InsertWithOffset(newMenuItem, before, 0);
}

void        MenuItemCollection::OnItemAdded(MenuItemRef newMenuItem, unsigned index)
{
    this->ItemAdded(newMenuItem, index);
}

void        MenuItemCollection::OnItemRemoved(MenuItemRef oldMenuItem)
{
    this->ItemRemoved(oldMenuItem);
}

void            MenuItemCollection::Remove(MenuItemRef toRemove)
{
    MenuItemList::iterator it =
        std::find(this->menuItemList.begin(), this->menuItemList.end(), toRemove);

    if (it == this->menuItemList.end())
    {
        throw InvalidMenuItemException();
    }

    this->menuItemList.erase(it);
    this->OnItemRemoved(toRemove);
}

unsigned         MenuItemCollection::Count()
{
    return (unsigned) this->menuItemList.size();
}

MenuItemRef MenuItemCollection::ItemAt(unsigned index)
{
    if (index >= this->menuItemList.size())
    {
        throw IndexOutOfRangeException();
    }

    return this->menuItemList.at(index);
}

MenuItemRef MenuItemCollection::operator[](unsigned index)
{
    return this->ItemAt(index);
}