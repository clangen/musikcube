#pragma once

#include "stdafx.h"
#include "ResourcesWindow.h"
#include "Screen.h"
#include "Colors.h"

#include <core/debug.h>
#include <boost/format.hpp>


ResourcesWindow::ResourcesWindow() {

    this->SetSize(Screen::GetWidth() / 2, 3);

    this->SetPosition(
        Screen::GetWidth() / 2, 
        Screen::GetHeight() - this->GetHeight());

    this->systemInfo = SystemInfo::Create();
    this->Create();
}

ResourcesWindow::~ResourcesWindow() {
    delete this->systemInfo;
}

#define BYTES_PER_MEGABYTE 1048576.0f

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