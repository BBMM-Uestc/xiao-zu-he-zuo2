#include "MainFrame.h"

#include <wx/wx.h>

namespace logicsim {

class LogicSimApp final : public wxApp {
public:
    bool OnInit() override {
        if (!wxApp::OnInit()) {
            return false;
        }

        auto* frame = new MainFrame();
        frame->Show();
        return true;
    }
};

}  // namespace logicsim

wxIMPLEMENT_APP(logicsim::LogicSimApp);
