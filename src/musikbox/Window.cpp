#pragma once

#include "stdafx.h"
#include "Window.h"

Window::Window() {
    this->frame = this->content = 0;
    this->height = 0;
    this->width = 0;
    this->x = 0;
    this->y = 0;
    this->contentColor = -1;
    this->frameColor = -1;
}

Window::~Window() {
    this->Destroy();
}

void Window::SetSize(int width, int height) {
    this->width = width;
    this->height = height;
}

void Window::SetPosition(int x, int y) {
    this->x = x;
    this->y = y;
}

int Window::GetWidth() const {
    return this->width;
}

int Window::GetHeight() const {
    return this->height;
}

int Window::GetContentHeight() const {
    return this->height - 2;
}

int Window::GetContentWidth() const {
    return this->width - 2;
}

int Window::GetX() const {
    return this->x;
}

int Window::GetY() const {
    return this->y;
}

void Window::SetContentColor(int color) {
    this->contentColor = color;

    if (this->contentColor != -1 && this->content) {
        wbkgd(this->content, COLOR_PAIR(this->contentColor));
        this->Repaint();
    }
}

void Window::SetFrameColor(int color) {
    this->frameColor = color;

    if (this->frameColor != -1 && this->frame) {
        wbkgd(this->frame, COLOR_PAIR(this->frameColor));
        this->Repaint();
    }
}

WINDOW* Window::GetContent() const {
    return this->content;
}

WINDOW* Window::GetFrame() const {
    return this->frame;
}

void Window::Create() {
    this->Destroy();

    this->frame = newwin(this->height, this->width, this->y, this->x);
    box(this->frame, 0, 0);
    wrefresh(this->frame);

    this->content = derwin(
        this->frame, 
        this->height - 2, 
        this->width - 2, 
        1, 
        1);

    if (this->contentColor != -1) {
        wbkgd(this->content, COLOR_PAIR(this->contentColor));
    }

    if (this->frameColor != -1) {
        wbkgd(this->frame, COLOR_PAIR(this->frameColor));
    }

    touchwin(this->content);
    wrefresh(this->content);
}

void Window::Clear() {
    wclear(this->content);
}

void Window::Repaint() {
    if (this->frame && this->content) {
        wrefresh(this->frame);
        wrefresh(this->content);
    }
}

void Window::Destroy() {
    if (this->frame) {
        delwin(this->content);
        delwin(this->frame);
        this->frame = this->content = NULL;
    }
}
