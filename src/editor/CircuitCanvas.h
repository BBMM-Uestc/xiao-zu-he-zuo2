#pragma once

#include "CircuitDocument.h"

#include <wx/wx.h>

namespace logicsim {

class CircuitCanvas final : public wxPanel {
public:
    explicit CircuitCanvas(wxWindow* parent);

    void SetDocument(CircuitDocument document);
    [[nodiscard]] const CircuitDocument& GetDocument() const noexcept;

private:
    void OnPaint(wxPaintEvent& event);

    CircuitDocument document_;
};

}  // namespace logicsim
