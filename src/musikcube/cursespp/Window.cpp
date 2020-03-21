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
#include <cursespp/Window.h>
#include <cursespp/IWindowGroup.h>
#include <cursespp/IInput.h>
#include <cursespp/ILayout.h>
#include <cursespp/Colors.h>
#include <cursespp/Screen.h>
#include <cursespp/Text.h>

#include <core/runtime/Message.h>
#include <core/runtime/MessageQueue.h>

using namespace cursespp;
using namespace musik::core::runtime;

static int NEXT_ID = 0;

static bool drawPending = false;
static bool freeze = false;
static Window* top = nullptr;
static Window* focused = nullptr;

static MessageQueue messageQueue;
static std::shared_ptr<INavigationKeys> keys;

#define ENABLE_BOUNDS_CHECK 1

/* clangen says: we used to just be able to call wbkgd() prior to ncurses 6.2;
however, something changed internally that causes the drawing to get corrupted
if we don't call wbkgdset() first. this looks like a bug, and the release notes
mention wbkgd() was changed, but it's unclear what exactly happened... */
#ifndef WIN32
    #define wbkgd_internal(window, color) \
        wbkgdset(window, color); \
        wbkgd(window, color);
#else
    #define wbkgd_internal(window, color) \
        wbkgd(window, color);
#endif

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

static inline void DrawTooSmall() {
    static const std::string error = "terminal too small";
    int64_t color = Color(Color::TextError);
    wclear(stdscr);
    wmove(stdscr, 0, 0);
    wattron(stdscr, color);
    waddstr(stdscr, error.c_str());
    wattroff(stdscr, color);
}

bool Window::WriteToScreen(IInput* input) {
    if (drawPending && !freeze) {
        drawPending = false;
        update_panels();
        doupdate();
        DrawCursor(input);
        return true;
    }
    else if (freeze) {
        drawPending = false;
        DrawTooSmall();
    }

    return false;
}

void Window::InvalidateScreen() {
    werase(stdscr);
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
    this->lastAbsoluteX = 0;
    this->lastAbsoluteY = 0;
    this->contentColor = Color(Color::ContentColorDefault);
    this->frameColor = Color(Color::FrameColorDefault);
    this->focusedContentColor = Color(Color::ContentColorDefault);
    this->focusedFrameColor = Color(Color::FrameColorFocused);
    this->drawFrame = true;
    this->isVisibleInParent = false;
    this->isDirty = true;
    this->focusOrder = -1;
    this->id = NEXT_ID++;
    this->badBounds = false;
}

Window::~Window() {
    messageQueue.Remove(this);
    if (::top == this) { top = nullptr; }
    if (::focused == this) { focused = nullptr; }
    this->Destroy();
}

int Window::GetId() const {
    return this->id;
}

void Window::ProcessMessage(musik::core::runtime::IMessage &message) {

}

bool Window::IsVisible() {
    return !this->badBounds && this->isVisibleInParent;
}

bool Window::IsFocused() {
    return ::focused == this;
}

void Window::BringToTop() {
    if (this->framePanel) {
        top_panel(this->framePanel);

        if (this->contentPanel && this->contentPanel != this->framePanel) {
            top_panel(this->contentPanel);
        }

        ::top = this;
    }
}

bool Window::IsTop() {
    return (::top == this);
}

void Window::SendToBottom() {
    if (this->framePanel) {
        bottom_panel(this->contentPanel);

        if (this->contentPanel != this->framePanel) {
            bottom_panel(this->framePanel);
        }
    }
}

void Window::Post(int messageType, int64_t user1, int64_t user2, int64_t delay) {
    messageQueue.Post(Message::Create(this, messageType, user1, user2), delay);
}

void Window::Broadcast(int messageType, int64_t user1, int64_t user2, int64_t delay) {
    messageQueue.Broadcast(Message::Create(nullptr, messageType, user1, user2), delay);
}

void Window::Debounce(int messageType, int64_t user1, int64_t user2, int64_t delay) {
    messageQueue.Debounce(Message::Create(this, messageType, user1, user2), delay);
}

void Window::Remove(int messageType) {
    messageQueue.Remove(this, messageType);
}

void Window::SetParent(IWindow* parent) {
    if (this->parent != parent) {
        IWindowGroup* group = dynamic_cast<IWindowGroup*>(this->parent);
        IWindow* oldParent = this->parent;
        bool visible = !!this->frame;

        if (visible) {
            this->Hide();
        }

        this->parent = parent;

        if (visible) {
            this->Show();
        }

        if (parent) {
            this->OnAddedToParent(parent);
        }
        else {
            this->OnRemovedFromParent(oldParent);
        }
    }
}

void Window::RecreateForUpdatedDimensions() {
    bool hasFrame = !!this->frame;
    if (hasFrame || this->isVisibleInParent) {
        this->Recreate();

        if (!hasFrame) {
            this->OnVisibilityChanged(true);
        }
    }

    this->OnDimensionsChanged();
}

void Window::MoveAndResize(int x, int y, int width, int height) {
    bool sizeChanged = this->width != width || this->height != height;

    this->x = x;
    this->y = y;
    int absX = this->GetAbsoluteX();
    int absY = this->GetAbsoluteY();

    bool positionChanged =
        absX != this->lastAbsoluteX ||
        absY != this->lastAbsoluteY;

    if (sizeChanged || positionChanged || this->badBounds || !this->content) {
        this->lastAbsoluteX = absX;
        this->lastAbsoluteY = absY;
        this->width = width;
        this->height = height;
        this->RecreateForUpdatedDimensions();
    }
    else {
        this->DestroyIfBadBounds();
    }
}

void Window::DestroyIfBadBounds() {
    if (this->CheckForBoundsError()) {
        this->badBounds = true;
        this->Destroy();
    }
}

void Window::SetSize(int width, int height) {
    if (this->width != width || this->height != height) {
        this->width = width;
        this->height = height;
        this->RecreateForUpdatedDimensions();
    }
    else {
        this->DestroyIfBadBounds();
    }
}

void Window::SetPosition(int x, int y) {
    this->x = x;
    this->y = y;
    int absX = this->GetAbsoluteX();
    int absY = this->GetAbsoluteY();

    if (absX != this->lastAbsoluteX || absY != this->lastAbsoluteY) {
        this->lastAbsoluteX = absX;
        this->lastAbsoluteY = absY;
        this->RecreateForUpdatedDimensions();
    }
    else {
        this->DestroyIfBadBounds();
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

void Window::OnAddedToParent(IWindow* newParent) {
    /* for subclass use */
}

void Window::OnRemovedFromParent(IWindow* oldParent) {
    /* for subclass use */
}

void Window::OnChildVisibilityChanged(bool visible, IWindow* child) {
    /* for subclass use */
}

void Window::DecorateFrame() {
    /* for subclass use */
}

void Window::Redraw() {
    this->isDirty = true;

    if (this->IsVisible() && this->IsParentVisible()) {
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

void Window::SetContentColor(Color color) {
    this->contentColor = (color == Color::Default)
        ? Color::ContentColorDefault : color;

    this->RepaintBackground();
    this->Redraw();
}

void Window::SetFocusedContentColor(Color color) {
    this->focusedContentColor = (color == Color::Default)
        ? Color::ContentColorDefault : color;

    this->RepaintBackground();
    this->Redraw();
}

void Window::SetFrameColor(Color color) {
    this->frameColor = (color == Color::Default)
        ? Color::FrameColorDefault : color;

    this->RepaintBackground();
    this->Redraw();
}

void Window::SetFocusedFrameColor(Color color) {
    this->focusedFrameColor = (color == Color::Default)
        ? Color::FrameColorFocused : color;

    this->RepaintBackground();
    this->Redraw();
}

void Window::DrawFrameAndTitle() {
    box(this->frame, 0, 0);

    /* draw the title, if one is specified */
    size_t titleLen = u8len(this->title);
    if (titleLen > 0) {
        int max = this->width - 4; /* 4 = corner + space + space + corner */
        if (max > 3) { /* 3 = first character plus ellipse (e.g. 'F..')*/
            std::string adjusted = " " + text::Ellipsize(this->title, (size_t) max - 2) + " ";
            wmove(this->frame, 0, 2);
            checked_waddstr(this->frame, adjusted.c_str());
        }
    }

    this->DecorateFrame();
}

void Window::RepaintBackground() {
    bool focused = IsFocused();

    if (this->drawFrame &&
        this->frameColor != Color::Default &&
        this->frame &&
        this->content != this->frame)
    {
        werase(this->frame);
        wbkgd_internal(this->frame, focused? this->focusedFrameColor : this->frameColor);
        this->DrawFrameAndTitle();
    }

    if (this->content) {
        werase(this->content);
        wbkgd_internal(this->content, focused ? this->focusedContentColor : this->contentColor);
    }

    this->Invalidate();
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
        this->isVisibleInParent = true;
        return;
    }

    bool notifyParent = false;

    if (this->badBounds) {
        if (!this->CheckForBoundsError()) {
            this->Recreate();
            this->badBounds = false;
            notifyParent = true;
        }
        this->isVisibleInParent = true;
    }
    else {
        if (this->framePanel) {
            if (!this->isVisibleInParent) {
                show_panel(this->framePanel);

                if (this->contentPanel && this->framePanel != this->contentPanel) {
                    show_panel(this->contentPanel);
                }

                this->isVisibleInParent = true;
                drawPending = true;
                notifyParent = true;
                this->OnVisibilityChanged(true);
            }
        }
        else {
            this->Create();
            notifyParent = true;
            this->isVisibleInParent = true;
        }

        if (this->isDirty) {
            this->Redraw();
        }
    }

    if (notifyParent && this->parent) {
        this->parent->OnChildVisibilityChanged(true, this);
    }
}

void Window::Recreate() {
    this->Destroy();
    this->Create();
}

void Window::OnParentVisibilityChanged(bool visible) {
    if (!visible && this->isVisibleInParent) {
        if (this->framePanel) {
            this->Destroy();
        }
    }
    else if (visible && this->isVisibleInParent) {
        if (this->framePanel) {
            this->Redraw();
        }
        else {
            this->Recreate();
        }
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

    if (this->drawFrame && (cx < 3 || cy < 3)) {
        /* if a frame is visible, the minimum x and y dimensions are
        2 cells -- the left+right (and top+bottom) border, plus the
        content area (at least 1 cell) */
        return true;
    }

    if (!this->parent) {
        return false;
    }

    const int parentWidth = this->parent->GetContentWidth();
    const int parentHeight = this->parent->GetContentHeight();

    if (this->width > parentWidth || this->height > parentHeight) {
        return true;
    }

    const int right = this->x + cx;
    const int bottom = this->y + cy;

    if (right > parentWidth || bottom > parentHeight) {
        return true;
    }

    return false;
}

int Window::GetAbsoluteX() const {
    return this->parent ? (this->x + parent->GetAbsoluteX()) : this->x;
}

int Window::GetAbsoluteY() const {
    return this->parent ? (this->y + parent->GetAbsoluteY()) : this->y;
}

void Window::SetFrameTitle(const std::string& title) {
    this->title = title;
    this->Destroy();
    this->Redraw();
}

std::string Window::GetFrameTitle() const {
    return this->title;
}

void Window::Create() {
    assert(this->frame == nullptr);
    assert(this->content == nullptr);
    assert(this->framePanel == nullptr);
    assert(this->contentPanel == nullptr);

    if (this->parent && !this->parent->IsVisible()) {
        return;
    }

    bool hadBadBounds = this->badBounds;
    this->badBounds = this->CheckForBoundsError();

    if (this->badBounds) {
        this->Destroy();
        return;
    }

    /* else we have valid bounds. this->x and this->y are specified in
    relative space; find their absolute offset based on our parent. */

    int absoluteXOffset = 0;
    int absoluteYOffset = 0;

    if (this->parent) {
        absoluteXOffset = this->parent->GetAbsoluteX();
        absoluteYOffset = this->parent->GetAbsoluteY();

        if (parent->IsFrameVisible()) {
            absoluteXOffset += 1;
            absoluteYOffset += 1;
        }
    }

    this->frame = newwin(
        this->height,
        this->width,
        absoluteYOffset + this->y,
        absoluteXOffset + this->x);

    if (this->frame) { /* can fail if the screen size is too small. */
        /* resolve our current colors colors */

        bool focused = this->IsFocused();

        int64_t currentFrameColor = focused
            ? this->focusedFrameColor : this->frameColor;

        int64_t currentContentColor = focused
            ? this->focusedContentColor : this->contentColor;

        /* create the corresponding panel. required for z-ordering. */

        this->framePanel = new_panel(this->frame);

        /* if we were asked not to draw a frame, we'll set the frame equal to
        the content view, and use the content views colors*/

        if (!this->drawFrame) {
            this->content = this->frame;
            this->RepaintBackground();
        }

        /* otherwise we'll draw a box around the frame, and create a content
        sub-window inside */

        else {
            this->content = newwin(
                this->height - 2,
                this->width - 2,
                absoluteYOffset + this->y + 1,
                absoluteXOffset + this->x + 1);

            if (!this->content) {
                /* should never happen. if there's enough room for this->frame,
                there's enough room for this->content */
                this->Destroy();
                return;
            }

            this->contentPanel = new_panel(this->content);
            this->RepaintBackground();
            this->DrawFrameAndTitle();
        }

        this->Show();

        if (hadBadBounds && this->isVisibleInParent) {
            this->OnVisibilityChanged(true);
            if (this->parent) {
                this->parent->OnChildVisibilityChanged(true, this);
            }
        }
    }
}

void Window::Hide() {
    bool notifyParent = false;
    this->Blur();
    if (this->frame) {
        if (this->isVisibleInParent) {
            this->Destroy();
            this->isVisibleInParent = false;
            this->OnVisibilityChanged(false);
            notifyParent = true;
        }
    }
    else {
        if (this->isVisibleInParent) {
            notifyParent = true;
            this->isVisibleInParent = false;
        }
    }

    if (notifyParent && this->parent) {
        this->parent->OnChildVisibilityChanged(false, this);
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

    bool focused = this->IsFocused();
    int64_t contentColor = focused ? this->focusedContentColor : this->contentColor;
    int64_t frameColor = focused ? this->focusedFrameColor : this->frameColor;

    if (this->content == this->frame) {
        wbkgd_internal(this->frame, contentColor);
    }
    else {
        wbkgd_internal(this->frame, frameColor);
        wbkgd_internal(this->content, contentColor);
    }
}

void Window::Invalidate() {
    if (this->isVisibleInParent) {
        if (this->frame) {
            drawPending = true;
        }
    }
}

bool Window::IsParentVisible() {
    if (this->parent == nullptr) {
        return true;
    }

    auto p = this->GetParent();
    while (p) {
        if (!p->IsVisible()) {
            return false;
        }
        p = p->GetParent();
    }

    return true;
}

void Window::Focus() {
    if (::focused != this) {
        if (::focused) { ::focused->Blur(); }
        ::focused = this;
        this->isDirty = true;
        this->OnFocusChanged(true);
        this->RepaintBackground();
        this->Redraw();
    }
}

void Window::Blur() {
    if (::focused == this) {
        ::focused = nullptr;
        this->isDirty = true;
        this->OnFocusChanged(false);
        this->RepaintBackground();
        this->Redraw();
    }
}

void Window::SetNavigationKeys(std::shared_ptr<INavigationKeys> keys) {
    ::keys = keys;
}

bool Window::MouseEvent(const IMouseHandler::Event& mouseEvent) {
    return false;
}

bool Window::FocusInParent() {
    auto layout = dynamic_cast<ILayout*>(this->GetParent());
    if (layout) {
        return layout->SetFocus(shared_from_this());
    }
    return false;
}

/* default keys for navigating around sub-views. apps can override this shim to
provide VIM-like keybindings, if it wants... */
static class DefaultNavigationKeys : public INavigationKeys {
    public:
        virtual bool Up(const std::string& key) override { return Up() == key; }
        virtual bool Down(const std::string& key) override { return Down() == key; }
        virtual bool Left(const std::string& key) override { return Left() == key; }
        virtual bool Right(const std::string& key) override { return Right() == key; }
        virtual bool Next(const std::string& key) override { return Next() == key; }
        virtual bool Prev(const std::string& key) override { return Prev() == key; }
        virtual bool Mode(const std::string& key) override { return Mode() == key; }
        virtual bool PageUp(const std::string& key) override { return PageUp() == key; }
        virtual bool PageDown(const std::string& key) override { return PageDown() == key; }
        virtual bool Home(const std::string& key) override { return Home() == key; }
        virtual bool End(const std::string& key) override { return End() == key; }

        virtual std::string Up() override { return "KEY_UP"; }
        virtual std::string Down() override { return "KEY_DOWN"; }
        virtual std::string Left() override { return "KEY_LEFT"; }
        virtual std::string Right() override { return "KEY_RIGHT"; }
        virtual std::string Next() override { return "KEY_TAB"; }
        virtual std::string Prev() override { return "KEY_BTAB"; }
        virtual std::string Mode() override  { return "^["; }
        virtual std::string PageUp() override { return "KEY_PPAGE"; }
        virtual std::string PageDown() override { return "KEY_NPAGE"; }
        virtual std::string Home() override { return "KEY_HOME"; }
        virtual std::string End() override { return "KEY_END"; }
} defaultNavigationKeys;

INavigationKeys& Window::NavigationKeys() {
    if (::keys) {
        return *::keys.get();
    }

    return defaultNavigationKeys;
}