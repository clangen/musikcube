#pragma once

#include "IScrollable.h"
#include "IScrollAdapter.h"
#include "ScrollableWindow.h"
#include <sigslot/sigslot.h>

namespace cursespp {
    class ListWindow : public ScrollableWindow {
        public:
            static size_t NO_SELECTION;

            sigslot::signal3<ListWindow*, size_t, size_t> SelectionChanged;
            sigslot::signal2<ListWindow*, size_t> Invalidated;

            ListWindow(IWindow *parent = NULL);
            virtual ~ListWindow();

            virtual void ScrollToTop();
            virtual void ScrollToBottom();
            virtual void ScrollUp(int delta = 1);
            virtual void ScrollDown(int delta = 1);
            virtual void PageUp();
            virtual void PageDown();

            virtual void Focus();
            virtual void Blur();

            virtual size_t GetSelectedIndex();

        protected:
            virtual void SetSelectedIndex(size_t index);
            virtual void OnAdapterChanged();
            virtual void OnSelectionChanged(size_t newIndex, size_t oldIndex);
            virtual void OnInvalidated();

            virtual IScrollAdapter::ScrollPosition& GetScrollPosition();

        private:
            IScrollAdapter::ScrollPosition scrollPosition;
            size_t selectedIndex;
    };
}
