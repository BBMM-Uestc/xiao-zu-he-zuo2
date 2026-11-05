#pragma once

#include <wx/wx.h>

namespace logicsim {

class CircuitCanvas;

class MainFrame final : public wxFrame {
public:
    MainFrame();

private:
    CircuitCanvas* canvas_{};
};

}  // namespace logicsim
