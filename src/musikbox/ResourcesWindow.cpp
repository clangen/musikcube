#pragma once

#include "stdafx.h"
#include "ResourcesWindow.h"
#include "Screen.h"
#include "Colors.h"

#include <core/debug.h>
#include <boost/format.hpp>

#define BYTES_PER_MEGABYTE 1048576.0f

ResourcesWindow::ResourcesWindow(IWindow *parent) 
: Window(parent) {
    this->systemInfo = SystemInfo::Create();
}

ResourcesWindow::~ResourcesWindow() {
    delete this->systemInfo;
}

void ResourcesWindow::Repaint() {
    this->Clear();

    float virtualMemoryUsed = (float) systemInfo->GetUsedVirtualMemory() / BYTES_PER_MEGABYTE;
    float physicalMemoryUsed = (float) systemInfo->GetUsedPhysicalMemory() / BYTES_PER_MEGABYTE;

    wprintw(
        this->GetContent(), 
        "cpu %.2f%% - virt %.2f (mb) - phys %.2f (mb)", 
        systemInfo->GetCpuUsage(),
        virtualMemoryUsed,
        physicalMemoryUsed);

    Window::Repaint();
}