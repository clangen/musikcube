//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, Casey Langen
//
// Sources and Binaries of: mC2, win32cpp
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

//////////////////////////////////////////////////////////////////////////////

#include "pch.hpp"
#include <cube/SourcesModel.hpp>

#include <core/PlaybackQueue.h>

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

class DummyItem: public SourcesItem, public EventHandler
{
private: /*ctor*/ DummyItem(const uistring& caption)
        : caption(caption)
        , view(uistring(caption + _T(" sources item")).c_str())
        {
            view.Created.connect(this, &DummyItem::OnViewCreated);
        }

public: void OnViewCreated(Window* window)
        {
            //this->view.SetBackgroundColor(Color(255, 255, 255));
        }

public: static SourcesItemRef Create(const uistring& caption)
        {
            return SourcesItemRef(new DummyItem(caption));
        }

public: virtual uistring Caption() { return this->caption; }
public: virtual Window*  View() { return &this->view; }

private: uistring caption;
private: Label view;
};

//////////////////////////////////////////////////////////////////////////////

#include <cube/BrowseView.hpp>
#include <cube/BrowseController.hpp>

class BrowseItem: public SourcesItem
{
private:    /*ctor*/ BrowseItem()
            : controller(view)
            {

            }

public:     /*dtor*/ ~BrowseItem()
            {
            }

public:     static SourcesItemRef Create()
            {
                return SourcesItemRef(new BrowseItem());
            }

public:     virtual uistring Caption() { return _T("Browse"); }
public:     virtual Window*  View()
            {
                return &this->view;
            }

private:    BrowseView view;
private:    BrowseController controller;
};

//////////////////////////////////////////////////////////////////////////////

class NowPlayingItem: public SourcesItem
{
private:    /*ctor*/ NowPlayingItem()
                : controller(view,NULL,musik::core::PlaybackQueue::Instance().NowPlayingTracklist())
            {
            }

public:     /*dtor*/ ~NowPlayingItem()
            {
            }

public:     static SourcesItemRef Create()
            {
                return SourcesItemRef(new NowPlayingItem());
            }

public:     virtual uistring Caption() { return _T("Now Playing"); }
public:     virtual Window*  View()
            {
                return &this->view;
            }

private:    TracklistView view;
private:    TracklistController controller;
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
#include <cube/SettingsView.hpp>
#include <cube/SettingsController.hpp>

class SettingsItem: public SourcesItem
{
private:    /*ctor*/ SettingsItem()
                : controller(view)
            {
            }

public:     /*dtor*/ ~SettingsItem()
            {
            }

public:     static SourcesItemRef Create()
            {
                return SourcesItemRef(new SettingsItem());
            }

public:     virtual uistring Caption() { return _T("Settings"); }
public:     virtual Window*  View()
            {
                return &this->view;
            }

private:    SettingsView view;
private:    SettingsController controller;
};

//////////////////////////////////////////////////////////////////////////////

typedef SourcesListModel::Category Category;
typedef SourcesListModel::CategoryRef CategoryRef;

/*ctor*/        SourcesModel::SourcesModel()
{
}

void            SourcesModel::Load()
{
    CategoryRef viewCategory(new Category(_T("View")));
    viewCategory->Add(BrowseItem::Create());
    viewCategory->Add(NowPlayingItem::Create());
    viewCategory->Add(SettingsItem::Create());
    this->AddCategory(viewCategory);

    CategoryRef playlistCategory(new Category(_T("Playlists")));
    playlistCategory->Add(DummyItem::Create(_T("Playlist 1")));
    playlistCategory->Add(DummyItem::Create(_T("Playlist 2")));
    playlistCategory->Add(DummyItem::Create(_T("Dynamic Playlist 3")));
    playlistCategory->Add(DummyItem::Create(_T("Dynamic Playlist 4")));
    this->AddCategory(playlistCategory);
}

#define FIND_CATEGORY(list, category) std::find(list.begin(), list.end(), category)

void            SourcesModel::AddCategory(CategoryRef category)
{
    if (FIND_CATEGORY(this->categories, category) != this->categories.end())
    {
        throw InvalidCategoryException();
    }

    this->categories.push_back(category);
    this->CategoryAdded(category);
}

void            SourcesModel::RemoveCategory(CategoryRef category)
{
    CategoryList::iterator it = FIND_CATEGORY(this->categories, category);
    if (it == this->categories.end())
    {
        throw InvalidCategoryException();
    }

    this->categories.erase(it);
    this->CategoryRemoved(category);
}
