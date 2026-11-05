#include "CircuitCanvas.h"

#include <wx/dcbuffer.h>
#include <wx/graphics.h>

#include <memory>
#include <utility>

namespace logicsim {

CircuitCanvas::CircuitCanvas(wxWindow* parent)
    : wxPanel(
          parent,
          wxID_ANY,
          wxDefaultPosition,
          wxDefaultSize,
          wxBORDER_SIMPLE | wxFULL_REPAINT_ON_RESIZE
      ) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(FromDIP(wxSize(640, 480)));

    Bind(wxEVT_PAINT, &CircuitCanvas::OnPaint, this);
    Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        Refresh();
        event.Skip();
    });
}

void CircuitCanvas::SetDocument(CircuitDocument document) {
    document_ = std::move(document);
    Refresh();
}

const CircuitDocument& CircuitCanvas::GetDocument() const noexcept {
    return document_;
}

void CircuitCanvas::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(wxColour(250, 250, 247)));
    dc.Clear();

    std::unique_ptr<wxGraphicsContext> graphics{
        wxGraphicsContext::Create(dc)
    };
    if (!graphics) {
        return;
    }

    const int width = GetClientSize().GetWidth();
    const int height = GetClientSize().GetHeight();
    const int gridSize = FromDIP(20);

    graphics->SetPen(wxPen(wxColour(224, 226, 221), 1));
    for (int x = 0; x <= width; x += gridSize) {
        graphics->StrokeLine(x, 0, x, height);
    }
    for (int y = 0; y <= height; y += gridSize) {
        graphics->StrokeLine(0, y, width, y);
    }

    const int componentWidth = FromDIP(78);
    const int componentHeight = FromDIP(44);
    const wxFont labelFont(wxFontInfo(10).FaceName("Consolas"));
    graphics->SetFont(labelFont, *wxBLACK);

    for (const auto& component : document_.components) {
        const double x = component.x;
        const double y = component.y;

        graphics->SetPen(wxPen(wxColour(54, 71, 79), 2));
        graphics->SetBrush(wxBrush(wxColour(255, 255, 255)));
        graphics->DrawRectangle(
            x,
            y,
            componentWidth,
            componentHeight
        );
        graphics->DrawText(
            wxString::FromUTF8(component.type),
            x + FromDIP(8),
            y + FromDIP(13)
        );
    }

    if (document_.components.empty()) {
        const wxFont hintFont(wxFontInfo(12));
        graphics->SetFont(hintFont, wxColour(95, 105, 110));
        graphics->DrawText(
            "Circuit canvas - place components here",
            FromDIP(28),
            FromDIP(28)
        );
    }
}

}  // namespace logicsim
