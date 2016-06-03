#pragma once

#include "ILayout.h"
#include "ILayoutStack.h"
#include "LayoutBase.h"

#include <memory>

namespace cursespp {
    class LayoutStack : public LayoutBase, public ILayoutStack {
        public:
            LayoutStack();
            virtual ~LayoutStack();

            /* ILayout */
            virtual IWindowPtr FocusNext();
            virtual IWindowPtr FocusPrev();
            virtual IWindowPtr GetFocus();
            virtual ILayoutStack* GetLayoutStack();
            virtual void SetLayoutStack(ILayoutStack* stack);
            virtual void Layout() { }

            /* IOrderable */
            virtual void BringToTop();
            virtual void SendToBottom();

            /* IDisplayable */
            virtual void Show();
            virtual void Hide();

            /* IKeyHandler */
            virtual bool KeyPress(const std::string& key);

            /* IWindowGroup */
            virtual bool AddWindow(IWindowPtr window);
            virtual bool RemoveWindow(IWindowPtr window);
            virtual size_t GetWindowCount();
            virtual IWindowPtr GetWindowAt(size_t position);

            /* ILayoutStack */
            virtual bool Push(ILayoutPtr layout);
            virtual bool Pop(ILayoutPtr layout);
            virtual bool BringToTop(ILayoutPtr layout);
            virtual bool SendToBottom(ILayoutPtr layout);
            virtual ILayoutPtr Top();

        private:
            std::deque<ILayoutPtr> layouts;
            ILayoutStack* stack;
            bool visible;
    };
}
