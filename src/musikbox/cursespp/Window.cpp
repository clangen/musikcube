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

#include <stdafx.h>
#include "Window.h"
#include "IWindowGroup.h"
#include "IInput.h"
#include "Colors.h"
#include "Screen.h"

#include <core/runtime/Message.h>
#include <core/runtime/MessageQueue.h>

using namespace cursespp;
using namespace musik::core::runtime;

static int NEXT_ID = 0;
static bool drawPending = false;
static bool freeze = false;

static MessageQueue messageQueue;

#define ENABLE_BOUNDS_CHECK 1

static inline void DrawCursor(IInput* input) {
    if (input) {
        Window* inputWindow = dynamic_cast<Window*>(input);
        if (inputWindow) {
            WINDOW* content = inputWindow->GetContent();
            curs_set(1);
            wtimeout(content, IDLE_TIMEOUT_MS);
            wmove(content, 0, input->Position());
        }
    }
    else {
        curs_set(0);
    }
}

void Window::WriteToScreen(IInput* input) {
    if (drawPending && !freeze) {
        drawPending = false;
        update_panels();
        doupdate();
        DrawCursor(input);
    }
}

void Window::InvalidateScreen() {
    wclear(stdscr);
    drawPending = true;
}

void Window::Freeze() {
    if (!freeze) {
        freeze = true;
        Window::InvalidateScreen();
    }
}

void Window::Unfreeze() {
    if (freeze) {
        freeze = false;
        Window::InvalidateScreen();
    }
}

IMessageQueue& Window::MessageQueue() {
    return messageQueue;
}

Window::Window(IWindow *parent) {
    this->frame = this->content = 0;
    this->framePanel = this->contentPanel = 0;
    this->parent = parent;
    this->height = 0;
    this->width = 0;
    this->x = 0;
    this->y = 0;
    this->contentColor = CURSESPP_DEFAULT_CONTENT_COLOR;
    this->frameColor = CURSESPP_DEFAULT_FRAME_COLOR;
    this->drawFrame = true;
    this->isVisible = false;
    this->isFocused = false;
    this->isDirty = true;
    this->focusOrder = -1;
    this->id = NEXT_ID++;
    this->badBounds = false;
}

Window::~Window() {
    messageQueue.Remove(this);
    this->Destroy();
}

int Window::GetId() const {
    return this->id;
}

void Window::ProcessMessage(musik::core::runtime::IMessage &message) {

}

bool Window::IsVisible() {
    return this->isVisible;
}

bool Window::IsFocused() {
    return this->isFocused;
}

void Window::BringToTop() {
    if (this->framePanel) {
        top_panel(this->framePanel);

        if (this->contentPanel != this->framePanel) {
            top_panel(this->contentPanel);
        }
    }
}

void Window::SendToBottom() {
    if (this->framePanel) {
        bottom_panel(this->contentPanel);

        if (this->contentPanel != this->framePanel) {
            bottom_panel(this->framePanel);
        }
    }
}

void Window::PostMessage(int messageType, int64 user1, int64 user2, int64 delay) {
    messageQueue.Post(
        Message::Create(
            this,
            messageType,
            user1,
            user2),
        delay);
}

void Window::DebounceMessage(int messageType, int64 user1, int64 user2, int64 delay) {
    messageQueue.Debounce(
        Message::Create(
            this,
            messageType,
            user1,
            user2),
        delay);
}

void Window::RemoveMessage(int messageType) {
    messageQueue.Remove(this, messageType);
}

void Window::SetParent(IWindow* parent) {
    if (this->parent != parent) {
        IWindowGroup* group = dynamic_cast<IWindowGroup*>(this->parent);

        if (group) {
            group->RemoveWindow(this->Window::shared_from_this());
        }

        this->parent = parent;

        if (this->frame) {
            this->Hide();
            this->Show();
        }
    }
}

void Window::RecreateForUpdatedDimensions() {
    bool hasFrame = !!this->frame;
    if (hasFrame || this->isVisible) {
        this->Recreate();

        if (!hasFrame) {
            this->OnVisibilityChanged(true);
        }
    }

    this->OnDimensionsChanged();
}

void Window::MoveAndResize(int x, int y, int width, int height) {
    bool sizeChanged = this->width != width || this->height != height;
    bool positionChanged = this->x != x || this->y != y;

    if (sizeChanged || positionChanged) {
        this->width = width;
        this->height = height;
        this->x = x;
        this->y = y;
        this->RecreateForUpdatedDimensions();
    }
}

void Window::SetSize(int width, int height) {
    if (this->width != width || this->height != height) {
        this->width = width;
        this->height = height;
        this->RecreateForUpdatedDimensions();
    }
}

void Window::SetPosition(int x, int y) {
    if (this->x != x || this->y != y) {
        this->x = x;
        this->y = y;
        this->RecreateForUpdatedDimensions();
    }
}

void Window::OnDimensionsChanged() {
    this->Invalidate();
}

void Window::OnVisibilityChanged(bool visible) {
    /* for subclass use */
}

void Window::OnFocusChanged(bool focused) {
    /* for subclass use */
}

void Window::OnRedraw() {
    /* for subclass use */
}

void Window::Redraw() {
    this->isDirty = true;

    if (this->IsVisible()) {
        if (this->parent && !this->parent->IsVisible()) {
            return;
        }

        if (!this->frame) {
            this->Create();
        }

        if (this->frame) {
            this->OnRedraw();
            this->Invalidate();
            this->isDirty = false;
            return;
        }
    }
}

int Window::GetWidth() const {
    return this->width;
}

int Window::GetHeight() const {
    return this->height;
}

int Window::GetContentHeight() const {
    if (!this->drawFrame) {
        return this->height;
    }

    return this->height ? this->height - 2 : 0;
}

int Window::GetContentWidth() const {
    if (!this->drawFrame) {
        return this->width;
    }

    return this->width ? this->width - 2 : 0;
}

int Window::GetX() const {
    return this->x;
}

int Window::GetY() const {
    return this->y;
}

void Window::SetContentColor(int64 color) {
    this->contentColor = (color == -1 ? CURSESPP_DEFAULT_CONTENT_COLOR : color);

    if (this->contentColor != -1 && this->content) {
        wbkgd(this->frame, COLOR_PAIR(this->frameColor));

        if (this->content != this->frame) {
            wbkgd(this->content, COLOR_PAIR(this->contentColor));
        }

        this->Invalidate();
    }
}

void Window::SetFrameColor(int64 color) {
    this->frameColor = (color == -1 ? CURSESPP_DEFAULT_FRAME_COLOR : color);

    if (this->drawFrame && this->frameColor != -1 && this->frame) {
        wbkgd(this->frame, COLOR_PAIR(this->frameColor));

        if (this->content != this->frame) {
            wbkgd(this->content, COLOR_PAIR(this->contentColor));
        }

        this->Invalidate();
    }
}

WINDOW* Window::GetContent() const {
    return this->content;
}

WINDOW* Window::GetFrame() const {
    return this->frame;
}

int Window::GetFocusOrder() {
    return this->focusOrder;
}

void Window::SetFocusOrder(int order) {
    this->focusOrder = order;
}

void Window::Show() {
    if (parent && !parent->IsVisible()) {
        /* remember that someone tried to make us visible, but don't do
        anything because we could corrupt the display */
        this->isVisible = true;
        return;
    }

    if (this->badBounds) {
        if (!this->CheckForBoundsError()) {
            this->Recreate();
            this->badBounds = false;
        }

        this->isVisible = true;
        return;
    }

    if (this->framePanel) {
        if (!this->isVisible) {
            show_panel(this->framePanel);

            if (this->framePanel != this->contentPanel) {
                show_panel(this->contentPanel);
            }

            this->isVisible = true;
            drawPending = true;

            this->OnVisibilityChanged(true);
        }
    }
    else {
        this->Create();
    }

    if (this->isDirty) {
        this->Redraw();
    }
}

void Window::Recreate() {
    this->Destroy();
    this->Create();
}

void Window::OnParentVisibilityChanged(bool visible) {
    if (!visible && this->isVisible) {
        if (this->framePanel) {
            this->Destroy();
        }

        this->OnVisibilityChanged(false);
    }
    else if (visible && this->isVisible) {
        if (this->framePanel) {
            this->Redraw();
        }
        else {
            this->Recreate();
        }

        this->OnVisibilityChanged(true);
    }
}

bool Window::CheckForBoundsError() {
    if (this->parent && ((Window *)this->parent)->CheckForBoundsError()) {
        return true;
    }

    int screenCy = Screen::GetHeight();
    int screenCx = Screen::GetWidth();

    int cx = this->GetWidth();
    int cy = this->GetHeight();

    if (cx <= 0 || cy <= 0) {
        return true;
    }

    if (cx > screenCx || cy > screenCy) {
        return true;
    }

    if (!this->parent) {
        return false;
    }

    int parentTop = parent->GetY();
    int parentBottom = parentTop + parent->GetHeight();
    int parentLeft = parent->GetX();
    int parentRight = parentLeft + parent->GetWidth();

    int top = this->GetY();
    int left = this->GetX();
    int bottom = top + cy;
    int right = left + cx;

    if (top < parentTop || bottom > parentBottom ||
        left < parentLeft || right > parentRight)
    {
        return true;
    }

    return false;
}

void Window::Create() {
    assert(this->frame == nullptr);
    assert(this->content == nullptr);
    assert(this->framePanel == nullptr);
    assert(this->contentPanel == nullptr);

    bool hadBadBounds = this->badBounds;
    this->badBounds = this->CheckForBoundsError();

    if (this->badBounds) {
        this->Destroy();
        return;
    }

    /* else we have valid bounds */

    this->frame = newwin(
        this->height,
        this->width,
        this->y,
        this->x);

    this->framePanel = new_panel(this->frame);

    /* if we were asked not to draw a frame, we'll set the frame equal to
    the content view, and use the content views colors*/

    if (!this->drawFrame) {
        this->content = this->frame;

        if (this->contentColor != -1) {
            wbkgd(this->frame, COLOR_PAIR(this->contentColor));
        }
    }

    /* otherwise we'll draw a box around the frame, and create a content
    sub-window inside */

    else {
        box(this->frame, 0, 0);

        this->content = newwin(
            this->height - 2,
            this->width - 2,
            this->GetY() + 1,
            this->GetX() + 1);

        this->contentPanel = new_panel(this->content);

        if (this->frameColor != -1) {
            wbkgd(this->frame, COLOR_PAIR(this->frameColor));
        }

        if (this->contentColor != -1) {
            wbkgd(this->content, COLOR_PAIR(this->contentColor));
        }
    }

    this->Show();

    if (hadBadBounds && this->isVisible) {
        this->OnVisibilityChanged(true);
    }
}

void Window::Hide() {
    if (this->frame) {
        if (this->isVisible) {
            this->Destroy();
            this->isVisible = false;
            this->OnVisibilityChanged(false);
        }
    }
    else {
        this->isVisible = false;
    }
}

void Window::Destroy() {
    if (this->frame) {
        del_panel(this->framePanel);
        delwin(this->frame);

        if (this->content != this->frame) {
            del_panel(this->contentPanel);
            delwin(this->content);
        }
    }

    this->framePanel = this->contentPanel = 0;
    this->content = this->frame = 0;
    this->isDirty = true;
}

void Window::SetFrameVisible(bool enabled) {
    if (enabled != this->drawFrame) {
        this->drawFrame = enabled;

        if (this->frame || this->content) {
            this->Destroy();
            this->Show();
        }
    }
}

bool Window::IsFrameVisible() {
    return this->drawFrame;
}

IWindow* Window::GetParent() const {
    return this->parent;
}

void Window::Clear() {
    werase(this->content);
    wmove(this->content, 0, 0);

    if (this->content == this->frame) {
        if (this->contentColor != -1) {
            wbkgd(this->frame, COLOR_PAIR(this->contentColor));
        }
        else {
            wbkgd(this->frame, COLOR_PAIR(this->frameColor));
        }
    }
    else {
        wbkgd(this->frame, COLOR_PAIR(this->frameColor));

        if (this->content != this->frame) {
            wbkgd(this->content, COLOR_PAIR(this->contentColor));
        }
    }
}

void Window::Invalidate() {
    if (this->isVisible) {
        if (this->frame) {
            drawPending = true;
        }
    }
}

void Window::Focus() {
    if (!this->isFocused) {
        this->isFocused = true;
        this->isDirty = true;
        this->OnFocusChanged(true);
        this->Redraw();
    }
}

void Window::Blur() {
    if (this->isFocused) {
        this->isFocused = false;
        this->isDirty = true;
        this->OnFocusChanged(false);
        this->Redraw();
    }
}
