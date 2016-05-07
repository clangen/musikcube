#pragma once

#include "stdafx.h"
#include "BorderedWindow.h"

BorderedWindow::BorderedWindow() {
    this->border = this->contents = 0;
    this->height = 0;
    this->width = 0;
    this->x = 0;
    this->y = 0;
    this->color = -1;
    this->scrollable = false;
}

BorderedWindow::~BorderedWindow() {
    this->Destroy();
}

void BorderedWindow::SetSize(int width, int height) {
    this->width = width;
    this->height = height;
}

void BorderedWindow::SetPosition(int x, int y) {
    this->x = x;
    this->y = y;
}

int BorderedWindow::GetWidth() const {
    return this->width;
}

int BorderedWindow::GetHeight() const {
    return this->height;
}

int BorderedWindow::GetX() const {
    return this->x;
}

int BorderedWindow::GetY() const {
    return this->y;
}

void BorderedWindow::SetColor(int color) {
    this->color = color;
}

void BorderedWindow::SetScrollable(bool scrollable) {
    this->scrollable = scrollable;
}

bool BorderedWindow::GetScrollable() const {
    return this->scrollable;
}

WINDOW* BorderedWindow::GetContents() const {
    return this->contents;
}

void BorderedWindow::Create() {
    this->Destroy();

    this->border = newwin(this->height, this->width, this->y, this->x);
    box(this->border, 0, 0);
    wrefresh(this->border);

    this->contents = derwin(
        this->border, 
        this->height - 2, 
        this->width - 2, 
        1, 
        1);

    scrollok(this->contents, this->scrollable);

    if (this->color != -1) {
        wbkgd(this->contents, COLOR_PAIR(this->color));
    }

    touchwin(this->contents);
    wrefresh(this->contents);
}

void BorderedWindow::Clear() {
    wclear(this->contents);
}

void BorderedWindow::Repaint() {
    if (this->border && this->contents) {
        wrefresh(this->border);
        wrefresh(this->contents);
    }
}

void BorderedWindow::Destroy() {
    if (this->border) {
        delwin(this->contents);
        delwin(this->border);
        this->border = this->contents = NULL;
    }
}
