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

#include "ResourcesWindow.h"

#include <cursespp/Screen.h>
#include <cursespp/Colors.h>

#include <boost/format.hpp>

#include <core/debug.h>

using namespace musik::box;
using namespace cursespp;

#define BYTES_PER_MEGABYTE 1048576.0f

ResourcesWindow::ResourcesWindow(IWindow *parent)
: Window(parent) {
    this->systemInfo = SystemInfo::Create();
}

ResourcesWindow::~ResourcesWindow() {
    delete this->systemInfo;
}

void ResourcesWindow::Update() {
    this->Clear();

    double virtualMemoryUsed = (double) systemInfo->GetUsedVirtualMemory() / BYTES_PER_MEGABYTE;
    double physicalMemoryUsed = (double) systemInfo->GetUsedPhysicalMemory() / BYTES_PER_MEGABYTE;
    double cpuUsage = (double) systemInfo->GetCpuUsage();

    wprintw(
        this->GetContent(),
        "cpu %.2f%% - virt %.2f (mb) - phys %.2f (mb)",
        cpuUsage,
        virtualMemoryUsed,
        physicalMemoryUsed);

    this->Repaint();
}
