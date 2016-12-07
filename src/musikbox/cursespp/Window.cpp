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
#include "Message.h"
#include "MessageQueue.h"
#include "Colors.h"
#include "Screen.h"

using namespace cursespp;

static int NEXT_ID = 0;
static bool drawPending = false;
static bool freeze = false;

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
    this->isVisible = this->isFocused = false;
    this->focusOrder = -1;
    this->id = NEXT_ID++;
    this->badBounds = false;
}

Window::~Window() {
    MessageQueue::Instance().Remove(this);
    this->Destroy();
}

int Window::GetId() const {
    return this->id;
}

void Window::ProcessMessage(IMessage &message) {

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
    MessageQueue::Instance().Post(
        Message::Create(
            this,
            messageType,
            user1,
            user2),
        delay);
}

void Window::DebounceMessage(int messageType, int64 user1, int64 user2, int64 delay) {
    MessageQueue::Instance().Debounce(
        Message::Create(
            this,
            messageType,
            user1,
            user2),
        delay);
}

void Window::RemoveMessage(int messageType) {
    MessageQueue::Instance().Remove(this, messageType);
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

void Window::MoveAndResize(int x, int y, int width, int height) {
    bool sizeChanged = this->width != width || this->height != height;
    bool positionChanged = this->x != x || this->y != y;

    if (sizeChanged || positionChanged) {
        this->width = width;
        this->height = height;
        this->x = x;
        this->y = y;

        if (this->frame) {
            this->Recreate();
        }

        if (sizeChanged || positionChanged) {
            this->OnDimensionsChanged();
        }
    }
}

void Window::SetSize(int width, int height) {
    if (this->width != width || this->height != height) {
        this->width = width;
        this->height = height;

        if (this->frame) {
            this->Recreate();
        }

        this->OnDimensionsChanged();
    }
}

void Window::SetPosition(int x, int y) {
    if (this->x != x || this->y != y) {
        this->x = x;
        this->y = y;

        if (this->frame) {
            this->Recreate();
        }

        this->OnDimensionsChanged();
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
    if (this->badBounds) {
        return;
    }
    else if (!this->badBounds && this->framePanel) {
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
}

void Window::Recreate() {
    this->Destroy();
    this->Create();
}

bool Window::CheckForBoundsError() {
    if (this->parent && ((Window *)this->parent)->HasBadBounds()) {
        return true;
    }

    int screenCy = Screen::GetHeight();
    int screenCx = Screen::GetWidth();

    int cx = this->GetWidth();
    int cy = this->GetHeight();

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
    /* see if the window is valid within the screen, and within it's parent.
    if not, draw it as a single cell red block. */

#if ENABLE_BOUNDS_CHECK > 0
    this->badBounds = this->CheckForBoundsError();

    if (this->badBounds) {
        this->frame = this->content = newwin(1, 1, 0, 0);
        wbkgd(this->frame, COLOR_PAIR(CURSESPP_BUTTON_NEGATIVE));
        this->framePanel = new_panel(this->frame);
        return;
    }
#endif

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
}

void Window::Hide() {
    if (this->frame) {
        if (this->isVisible) {
            hide_panel(this->framePanel);

            if (this->content != this->frame) {
                hide_panel(this->contentPanel);
            }

            this->isVisible = false;
            this->OnVisibilityChanged(false);
        }
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
        this->OnFocusChanged(true);
    }
}

void Window::Blur() {
    if (this->isFocused) {
        this->isFocused = false;
        this->OnFocusChanged(false);
    }
}
