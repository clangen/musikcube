#pragma once

namespace cursespp {
    class IDisplayable {
        public:
            virtual ~IDisplayable() { }
            virtual void Show() = 0;
            virtual void Hide() = 0;
    };
}
