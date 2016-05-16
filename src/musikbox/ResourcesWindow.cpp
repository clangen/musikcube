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