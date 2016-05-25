#pragma once

#include "IWindowGroup.h"
#include "IDisplayable.h"
#include "IKeyHandler.h"
#include <memory>

namespace cursespp {
    class ILayoutStack;

    class ILayout : public IWindowGroup, public IKeyHandler, public IOrderable, public IDisplayable {
        public:
            virtual ~ILayout() = 0 { }
            virtual IWindowPtr FocusNext() = 0;
            virtual IWindowPtr FocusPrev() = 0;
            virtual IWindowPtr GetFocus() = 0;
            virtual ILayoutStack* GetLayoutStack() = 0;
            virtual void SetLayoutStack(ILayoutStack* stack) = 0;
            virtual void Layout() = 0;
    };

    typedef std::shared_ptr<ILayout> ILayoutPtr;
}