#include "MainFrame.h"

#include "CircuitCanvas.h"

namespace logicsim {

MainFrame::MainFrame()
    : wxFrame(
          nullptr,
          wxID_ANY,
          "LogicSim",
          wxDefaultPosition,
          wxSize(1100, 760)
      ) {
    auto* fileMenu = new wxMenu();
    fileMenu->Append(wxID_EXIT, "E&xit");
    Bind(
        wxEVT_MENU,
        [this](wxCommandEvent&) { Close(true); },
        wxID_EXIT
    );

    auto* menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, "&File");
    SetMenuBar(menuBar);

    CreateStatusBar();
    SetStatusText("Ready - project skeleton");

    canvas_ = new CircuitCanvas(this);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(canvas_, 1, wxEXPAND | wxALL, FromDIP(8));
    SetSizer(sizer);

    Centre();
}

}  // namespace logicsim
