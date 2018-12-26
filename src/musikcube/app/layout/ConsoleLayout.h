#pragma once

#include <cursespp/LayoutBase.h>
#include <cursespp/ListWindow.h>
#include <cursespp/ITopLevelLayout.h>
#include <app/util/ConsoleLogger.h>

namespace musik { namespace cube {

    class ConsoleLayout:
        public cursespp::LayoutBase,
        public cursespp::ITopLevelLayout,
        public sigslot::has_slots<>
    {
        public:
            ConsoleLayout(ConsoleLogger* logger);

            virtual void OnLayout() override;
            virtual void SetShortcutsWindow(cursespp::ShortcutsWindow* w) override;

        protected:
            void OnVisibilityChanged(bool visible) override;

        private:
            void OnAdapterChanged(cursespp::SimpleScrollAdapter* adapter);
            void OnSelectionChanged(cursespp::ListWindow* window, size_t index, size_t prev);
            void OnItemActivated(cursespp::ListWindow* window, size_t index);

            ConsoleLogger* logger;
            ConsoleLogger::AdapterPtr adapter;
            std::shared_ptr<cursespp::ListWindow> listView;
            cursespp::ShortcutsWindow* shortcuts;
            bool scrolledToBottom { true };
    };

} }