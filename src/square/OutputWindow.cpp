#pragma once

#include "stdafx.h"
#include "OutputWindow.h"
#include "Screen.h"
#include "Colors.h"

OutputWindow::OutputWindow() {
    this->SetSize(Screen::GetWidth() / 2, Screen::GetHeight() - 3 - 4);
    this->SetPosition(0, 4);
    this->SetColor(BOX_COLOR_BLACK_ON_GREY);
    this->SetScrollable(true);
    this->Create();
}

OutputWindow::~OutputWindow() {

}

void OutputWindow::Write(const std::string& text) {
    wprintw(this->GetContents(), "%s", text.c_str());
    this->Repaint();
}


void OutputWindow::WriteLine(const std::string& line) {
    this->Write(line + "\n");
}