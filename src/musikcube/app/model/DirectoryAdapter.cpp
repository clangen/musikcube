//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include "stdafx.h"

#include <core/support/Common.h>

#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/SingleLineEntry.h>

#include "DirectoryAdapter.h"

using namespace musik::cube;
using namespace cursespp;
using namespace boost::filesystem;

#ifdef WIN32
void buildDriveList(std::vector<std::string>& target) {
    target.clear();
    static char buffer[4096];
    DWORD result = ::GetLogicalDriveStringsA(4096, buffer);
    if (result) {
        char* current = buffer;
        while (*current) {
            target.push_back(std::string(current));
            current += strlen(current) + 1;
        }
    }
}
#endif

void buildDirectoryList(
    const path& p,
    std::vector<std::string>& target,
    bool showDotfiles)
{
    target.clear();

    try {
        directory_iterator end;
        directory_iterator file(p);

        while (file != end) {
            if (is_directory(file->status())) {
                std::string leaf = file->path().leaf().string();
                if (showDotfiles || leaf[0] != '.') {
                    target.push_back(leaf);
                }
            }
            ++file;
        }
    }
    catch (...) {
        /* todo: log maybe? */
    }

    std::sort(target.begin(), target.end(), std::locale(""));
}

DirectoryAdapter::DirectoryAdapter() {
    this->showDotfiles = false;
    this->dir = musik::core::GetHomeDirectory();
    buildDirectoryList(dir, subdirs, showDotfiles);
}

DirectoryAdapter::~DirectoryAdapter() {

}

size_t DirectoryAdapter::Select(cursespp::ListWindow* window, size_t index) {
    bool hasParent = dir.has_parent_path();
    size_t selectedIndex = 0;

    if (hasParent && index == 0) {
        if (selectedIndexStack.size()) {
            selectedIndex = this->selectedIndexStack.top();
            this->selectedIndexStack.pop();
        }

        this->dir = this->dir.parent_path();
    }
    else {
        selectedIndexStack.push(window->GetSelectedIndex());
        dir /= this->subdirs[hasParent ? index - 1 : index];
    }

#ifdef WIN32
    std::string pathstr = this->dir.string();
    if ((pathstr.size() == 2 && pathstr[1] == ':') ||
        (pathstr.size() == 3 && pathstr[2] == ':'))
    {
        dir = path();
        buildDriveList(subdirs);
        return selectedIndex;
    }

#endif

    buildDirectoryList(dir, subdirs, showDotfiles);

    return selectedIndex;
}

std::string DirectoryAdapter::GetFullPathAt(size_t index) {
    bool hasParent = dir.has_parent_path();

    if (hasParent && index == 0) {
        return "";
    }

    index = (hasParent ? index - 1 : index);
    return (dir / this->subdirs[index]).string();
}

size_t DirectoryAdapter::GetEntryCount() {
    size_t count = subdirs.size();
    return dir.has_parent_path() ? count + 1 : count;
}

void DirectoryAdapter::SetDotfilesVisible(bool visible) {
    if (showDotfiles != visible) {
        showDotfiles = visible;
        buildDirectoryList(dir, subdirs, showDotfiles);
    }
}

IScrollAdapter::EntryPtr DirectoryAdapter::GetEntry(cursespp::ScrollableWindow* window, size_t index) {
    if (dir.has_parent_path()) {
        if (index == 0) {
            return IScrollAdapter::EntryPtr(new SingleLineEntry(".."));
        }
        --index;
    }

    return IScrollAdapter::EntryPtr(new SingleLineEntry(this->subdirs[index]));
}
