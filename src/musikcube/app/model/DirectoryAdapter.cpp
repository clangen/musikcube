//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <stdafx.h>

#include <core/support/Common.h>
#include <cursespp/Text.h>
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/SingleLineEntry.h>

#include "DirectoryAdapter.h"

using namespace musik::cube;
using namespace cursespp;
using namespace boost::filesystem;

#ifdef WIN32
static void buildDriveList(std::vector<std::string>& target) {
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

static bool shouldBuildDriveList(const boost::filesystem::path& dir) {
    std::string pathstr = dir.string();
    return
        (pathstr.size() == 2 && pathstr[1] == ':') ||
        (pathstr.size() == 3 && pathstr[2] == ':') ||
        (pathstr.size() == 0);
}
#endif

static bool hasSubdirectories(
    boost::filesystem::path p, bool showDotfiles)
{
    try {
        directory_iterator end;
        directory_iterator file(p);

        while (file != end) {
            if (is_directory(file->status())) {
                if (showDotfiles || file->path().leaf().string()[0] != '.') {
                    return true;
                }
            }
            ++file;
        }
    }
    catch (...) {
    }

    return false;
}

static void buildDirectoryList(
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
    }

    try {
        std::sort(
            target.begin(),
            target.end(),
            std::locale(setlocale(LC_ALL, nullptr)));
    }
    catch (...) {
        std::sort(target.begin(), target.end());
    }
}

static std::string normalizePath(boost::filesystem::path path) {
    return musik::core::NormalizeDir(path.string());
}

DirectoryAdapter::DirectoryAdapter() {
    this->showDotfiles = false;
    this->dir = musik::core::GetHomeDirectory();
    buildDirectoryList(dir, subdirs, showDotfiles);
}

DirectoryAdapter::~DirectoryAdapter() {

}

void DirectoryAdapter::SetAllowEscapeRoot(bool allow) {
    this->allowEscapeRoot = allow;
}

size_t DirectoryAdapter::Select(cursespp::ListWindow* window) {
    bool hasParent = this->ShowParentPath();
    size_t selectedIndex = NO_INDEX;
    size_t initialIndex = window->GetSelectedIndex();

    if (hasParent && initialIndex == 0) {
        if (selectedIndexStack.size()) {
            selectedIndex = this->selectedIndexStack.top();
            this->selectedIndexStack.pop();
        }

        this->dir = this->dir.parent_path();
    }
    else {
        selectedIndexStack.push(initialIndex);
        this->dir /= this->subdirs[hasParent ? initialIndex - 1 : initialIndex];
    }

#ifdef WIN32
    if (shouldBuildDriveList(this->dir)) {
        dir = path();
        buildDriveList(subdirs);
        return selectedIndex;
    }
#endif

    buildDirectoryList(dir, subdirs, showDotfiles);
    window->OnAdapterChanged();

    return selectedIndex;
}

void DirectoryAdapter::SetRootDirectory(const std::string& directory) {
    if (directory.size()) {
        dir = rootDir = directory;
    }
    else {
        dir = musik::core::GetHomeDirectory();
        rootDir = boost::filesystem::path();
    }

    buildDirectoryList(dir, subdirs, showDotfiles);
}

std::string DirectoryAdapter::GetFullPathAt(size_t index) {
    bool hasParent = this->ShowParentPath();
    if (hasParent && index == 0) {
        return "";
    }

    index = (hasParent ? index - 1 : index);
    return normalizePath((dir / this->subdirs[index]).string());
}

std::string DirectoryAdapter::GetLeafAt(size_t index) {
    if (this->ShowParentPath() && index == 0) {
        return "..";
    }

    return this->subdirs[index];
}

size_t DirectoryAdapter::IndexOf(const std::string& leaf) {
   for (size_t i = 0; i < this->subdirs.size(); i++) {
        if (this->subdirs[i] == leaf) {
            return i;
        }
    }

   return NO_INDEX;
}

size_t DirectoryAdapter::GetEntryCount() {
    size_t count = subdirs.size();
    return this->ShowParentPath() ? count + 1 : count;
}

void DirectoryAdapter::SetDotfilesVisible(bool visible) {
    if (showDotfiles != visible) {
        showDotfiles = visible;
#ifdef WIN32
        if (shouldBuildDriveList(this->dir)) {
            buildDriveList(subdirs);
            return;
        }
#endif
        buildDirectoryList(dir, subdirs, showDotfiles);
    }
}

std::string DirectoryAdapter::GetParentPath() {
    if (dir.has_parent_path() &&
        normalizePath(this->dir) != normalizePath(this->rootDir))
    {
        return normalizePath(dir.parent_path().string());
    }

    return "";
}

std::string DirectoryAdapter::GetCurrentPath() {
    return normalizePath(dir.string());
}

void DirectoryAdapter::Refresh() {
    buildDirectoryList(dir, subdirs, showDotfiles);
}

bool DirectoryAdapter::ShowParentPath() {
    if (normalizePath(this->dir) == normalizePath(this->rootDir)
        && !this->allowEscapeRoot)
    {
        return false;
    }

    return dir.has_parent_path();
}

bool DirectoryAdapter::HasSubDirectories(size_t index) {
    bool hasParent = this->ShowParentPath();
    if (index == 0 && hasParent) {
        return true;
    }

    index = hasParent ? index - 1 : index;
    return hasSubdirectories(this->dir / this->subdirs[index], this->showDotfiles);
}

bool DirectoryAdapter::HasSubDirectories() {
    return hasSubdirectories(this->dir, this->showDotfiles);
}

IScrollAdapter::EntryPtr DirectoryAdapter::GetEntry(cursespp::ScrollableWindow* window, size_t index) {
    if (this->ShowParentPath()) {
        if (index == 0) {
            return IScrollAdapter::EntryPtr(new SingleLineEntry(".."));
        }
        --index;
    }

    auto text = text::Ellipsize(this->subdirs[index], this->GetWidth());
    return IScrollAdapter::EntryPtr(new SingleLineEntry(text));
}
