export module GW2Viewer.UI.Controls:SearchInput;
import :AsyncProgressBar;
import GW2Viewer.UI.ImGui;
import GW2Viewer.Utils.Async;
import std;
#include "Macros.h"

export namespace GW2Viewer::UI::Controls
{

template<typename FilteredList>
bool SearchInput(std::string& filterString, FilteredList& filteredList, std::shared_mutex& lock, Utils::Async::Scheduler* asyncFilter = nullptr)
{
    I::SetNextItemWidth(-FLT_MIN);
    bool const result = I::InputTextWithHint("##Search", ICON_FA_MAGNIFYING_GLASS " Search...", &filterString);

    if (asyncFilter)
        AsyncProgressBar(*asyncFilter);

    std::shared_lock _(lock);
    auto rect = I::LastRect();
    rect.Max.x -= I::GetStyle().FramePadding.x;
    I::AlignTextToFramePadding();
    I::RightAlignedText(rect, "<c=#8>{} result{}</c>", filteredList.size(), filterString.size() != 1 ? "s" : "");

    return result;
}

}
