#pragma once

#include <stdafx.h>
#include "ILayout.h"

namespace cursespp {
    class ILayoutStack {
        public:
            virtual ~ILayoutStack() { }
            virtual bool Push(ILayoutPtr layout) = 0;
            virtual bool Pop(ILayoutPtr layout) = 0;
            virtual bool BringToTop(ILayoutPtr layout) = 0;
            virtual bool SendToBottom(ILayoutPtr layout) = 0;
    };
}
