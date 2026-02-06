export module GW2Viewer.UI.Viewers.VendorListViewer;
import GW2Viewer.Common;
import GW2Viewer.Common.Time;
import GW2Viewer.Content;
import GW2Viewer.Content.Vendor;
import GW2Viewer.Data.Game;
import GW2Viewer.UI.Controls;
import GW2Viewer.UI.ImGui;
import GW2Viewer.UI.Manager;
import GW2Viewer.UI.Viewers.ListViewer;
import GW2Viewer.UI.Viewers.VendorViewer;
import GW2Viewer.UI.Viewers.ViewerRegistry;
import GW2Viewer.Utils.Async;
import GW2Viewer.Utils.Encoding;
import GW2Viewer.Utils.Format;
import GW2Viewer.Utils.Scan;
import GW2Viewer.Utils.Sort;
import GW2Viewer.Utils.String;
import std;
#include "Macros.h"

export namespace GW2Viewer::UI::Viewers
{

struct VendorListViewer : ListViewer<VendorListViewer, { ICON_FA_SACK " Vendors", "Vendors", Category::ListViewer }>
{
    VendorListViewer(uint32 id, bool newTab) : Base(id, newTab) { UpdateSearch(); }

    std::shared_mutex Lock;
    std::vector<uint64> FilteredList;
    Utils::Async::Scheduler AsyncFilter { true };

    std::string FilterString;
    std::optional<std::pair<int64, int64>> FilterID;
    uint64 FilterRange { };
    enum class VendorSort { Hash, Name, TabNames, EncounteredTime } Sort { VendorSort::Hash };
    bool SortInvert { };

    static auto SortList(Utils::Async::Context context, std::vector<uint64>& data, VendorSort sort, bool invert)
    {
        std::shared_lock _(Content::vendorsLock);
        switch (sort)
        {
            using Utils::Sort::ComplexSort;
            using enum VendorSort;
            case Hash:
                std::ranges::sort(data, [invert](auto a, auto b) { return a < b ^ invert; });
                break;
            case Name:
                ComplexSort(data, invert, [](uint64 id) { return Content::vendors.at(id).Name(); });
                break;
            case TabNames:
                ComplexSort(data, invert, [](uint64 id) { return Content::vendors.at(id).TabNames(); });
                break;
            case EncounteredTime:
                ComplexSort(data, invert, [](uint64 id) { return Content::vendors.at(id).Encounter.Time; });
                break;
            default: std::terminate();
        }
    }
    void SetResult(Utils::Async::Context context, std::vector<uint64>&& data)
    {
        if (context->Cancelled) return;
        std::unique_lock _(Lock);
        FilteredList = std::move(data);
        context->Finish();
    }
    void UpdateSort() override
    {
        if (std::shared_lock _(Lock); FilteredList.empty())
            return UpdateSearch();

        AsyncFilter.Run([this, sort = Sort, invert = SortInvert](Utils::Async::Context context)
        {
            context->SetIndeterminate();
            std::vector<uint64> data;
            {
                std::shared_lock _(Lock);
                data = FilteredList;
            }
            CHECK_ASYNC;
            SortList(context, data, sort, invert);
            SetResult(context, std::move(data));
        });
    }
    void UpdateSearch() override
    {
        bool textSearch = false;
        FilterID.reset();
        if (FilterString.empty())
            ;
        else if (uint64 id, range; Utils::Scan::Into(FilterString, "{}+{}", id, range))
            FilterID.emplace(id - range, id + range);
        else if (Utils::Scan::Into(FilterString, "{}-{}", id, range))
            FilterID.emplace(id, range);
        else if (Utils::Scan::NumberLiteral(FilterString, id))
            FilterID.emplace(id - FilterRange, id + FilterRange);
        else if (Utils::Scan::NumberLiteral(FilterString, "0x{:x}", id))
            FilterID.emplace(id - FilterRange, id + FilterRange);
        else
            textSearch = true;

        AsyncFilter.Run([this, textSearch, filter = FilterID, string = FilterString, sort = Sort, invert = SortInvert](Utils::Async::Context context) mutable
        {
            context->SetIndeterminate();
            std::vector<uint64> data;
            CHECK_ASYNC;
            if (textSearch)
            {
                std::shared_lock _(Content::vendorsLock);
                data.assign_range(Content::vendors | std::views::keys | std::views::filter([query = Utils::String::Uppercased(Utils::Encoding::FromUTF8(string))](uint64 hash)
                {
                    auto const& vendor = Content::vendors.at(hash);
                    if (auto const string = G::Game.Text.GetNormalized(vendor.NameTextID).first; string && !string->empty() && string->contains(query))
                        return true;
                    for (auto const& tab : vendor.ServiceTabs)
                    {
                        if (auto const string = G::Game.Text.GetNormalized(tab.NameTextID).first; string && !string->empty() && string->contains(query))
                            return true;
                        for (auto const& item : tab.InventoryItems)
                        {
                            if (auto const itemDef = G::Game.Content.GetByDataID(Content::ItemDef, item.ItemDefDataID))
                                if (Utils::String::Uppercased(itemDef->GetDisplayName(false, true)).contains(query))
                                    return true;
                            if (auto const string = G::Game.Text.GetNormalized(item.UnlockTextID).first; string && !string->empty() && string->contains(query))
                                return true;
                        }
                    }
                    return false;
                }));
            }
            else
            {
                auto limits = filter.value_or(std::pair { std::numeric_limits<int64>::min(), std::numeric_limits<int64>::max() });
                std::shared_lock _(Content::vendorsLock);
                data.assign_range(Content::vendors | std::views::filter([limits](auto const& pair) { return (int64)pair.first >= limits.first && (int64)pair.first <= limits.second; }) | std::views::keys);
            }
            CHECK_ASYNC;
            SortList(context, data, sort, invert);
            SetResult(context, std::move(data));
        });
    }

    void Draw() override
    {
        if (Controls::SearchInput(FilterString, FilteredList, Lock, &AsyncFilter))
            UpdateSearch();

        if (scoped::TableList("Table", 4))
        {
            I::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 50, (ImGuiID)VendorSort::Hash);
            I::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1, (ImGuiID)VendorSort::Name);
            I::TableSetupColumn("Service Tabs", ImGuiTableColumnFlags_WidthStretch, 3, (ImGuiID)VendorSort::TabNames);
            I::TableSetupColumn("Encountered", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 80, (ImGuiID)VendorSort::EncounteredTime);
            I::TableSetupScrollFreeze(0, 1);
            I::TableHeadersRow();
            HandleTableSort(Sort, SortInvert);

            std::shared_lock __(Lock);
            ImGuiListClipper clipper;
            clipper.Begin(FilteredList.size());
            while (clipper.Step())
            {
                for (auto vendorHash : std::span(FilteredList.begin() + clipper.DisplayStart, FilteredList.begin() + clipper.DisplayEnd))
                {
                    scoped::WithID(vendorHash);

                    std::shared_lock ___(Content::vendorsLock);
                    auto& vendor = Content::vendors.at(vendorHash);
                    auto const* currentViewer = G::UI.GetCurrentViewer<VendorViewer>();
                    I::TableNextRow();

                    I::TableNextColumn();
                    I::Selectable(std::format("  <c=#4>{}</c>", vendorHash).c_str(), currentViewer && currentViewer->VendorHash == vendorHash ? ImGuiTreeNodeFlags_Selected : 0, ImGuiSelectableFlags_SpanAllColumns);

                    Controls::CompletionMargin(vendor.GetCompleteness());

                    if (auto const button = I::IsItemMouseClickedWith(ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle))
                        VendorViewer::Open(vendorHash, { .MouseButton = button });

                    I::TableNextColumn();
                    I::TextUnformatted(vendor.Name().c_str());

                    I::TableNextColumn();
                    I::TextUnformatted(vendor.TabNames().c_str());

                    I::TableNextColumn();
                    Controls::Encounter(vendor.Encounter);
                }
            }
        }
    }
};

}
