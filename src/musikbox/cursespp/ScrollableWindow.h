#pragma once

#include "curses_config.h"
#include "Window.h"
#include "IScrollAdapter.h"
#include "IScrollable.h"
#include "IKeyHandler.h"

namespace cursespp {
    class ScrollableWindow : public IScrollable, public IKeyHandler, public Window {
        public:
            ScrollableWindow(IWindow *parent = NULL);
            virtual ~ScrollableWindow();

            virtual void Show();
            virtual void SetSize(int width, int height);

            virtual bool KeyPress(const std::string& key);

            virtual void ScrollToTop();
            virtual void ScrollToBottom();
            virtual void ScrollUp(int delta = 1);
            virtual void ScrollDown(int delta = 1);
            virtual void PageUp();
            virtual void PageDown();

        protected:
            virtual IScrollAdapter& GetScrollAdapter() = 0;
            virtual IScrollAdapter::ScrollPosition& GetScrollPosition();
            virtual void OnAdapterChanged();

            size_t GetPreviousPageEntryIndex();
            bool IsLastItemVisible();

        private:
            IScrollAdapter::ScrollPosition scrollPosition;
    };
}
