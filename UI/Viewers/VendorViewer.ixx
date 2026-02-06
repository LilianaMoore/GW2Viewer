export module GW2Viewer.UI.Viewers.VendorViewer;
import GW2Viewer.Common;
import GW2Viewer.UI.Viewers.ViewerRegistry;
import GW2Viewer.UI.Viewers.ViewerWithHistory;
import std;
#include "Macros.h"

export namespace GW2Viewer::UI::Viewers
{

struct VendorViewer : ViewerWithHistory<VendorViewer, uint64, { ICON_FA_SACK " Vendor", "Vendor", Category::ObjectViewer }>
{
    TargetType VendorHash;

    VendorViewer(uint32 id, bool newTab, TargetType vendorHash) : Base(id, newTab), VendorHash(vendorHash) { }

    TargetType GetCurrent() const override { return VendorHash; }
    bool IsCurrent(TargetType target) const override { return VendorHash == target; }

    std::string Title() override;
    void Draw() override;
};

}
