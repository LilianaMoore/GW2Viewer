export module GW2Viewer.UI.Viewers.StringListViewer;
import GW2Viewer.Common;
import GW2Viewer.Common.Time;
import GW2Viewer.Data.Encryption;
import GW2Viewer.Data.Encryption.Text;
import GW2Viewer.Data.Game;
import GW2Viewer.UI.Controls;
import GW2Viewer.UI.ImGui;
import GW2Viewer.UI.Manager;
import GW2Viewer.UI.Viewers.ListViewer;
import GW2Viewer.UI.Viewers.ViewerRegistry;
import GW2Viewer.UI.Windows.ContentSearch;
import GW2Viewer.User.Config;
import GW2Viewer.Utils.Async;
import GW2Viewer.Utils.Encoding;
import GW2Viewer.Utils.Scan;
import GW2Viewer.Utils.Sort;
import GW2Viewer.Utils.String;
import GW2Viewer.Utils.Thread;
import std;
import <ctype.h>;
#include "Macros.h"

export namespace GW2Viewer::UI::Viewers
{

struct StringListViewer : ListViewer<StringListViewer, { ICON_FA_TEXT " Strings", "Strings", Category::ListViewer }>
{
    std::shared_mutex Lock;
    std::vector<uint32> FilteredList;
    Utils::Async::Scheduler AsyncFilter { true };

    std::string FilterString;
    std::optional<std::pair<int32, int32>> FilterID;
    uint32 FilterRange { };
    struct { bool Unencrypted = true, Encrypted = true, Decrypted = true, Empty = true; } Filters;

    struct DrawContext
    {
        uint32 StringID;
        std::wstring const* String;
        Data::Encryption::Status Status;
        Data::Encryption::TextKeyInfo* KeyInfo = G::Game.Encryption.GetTextKeyInfo(StringID);

        DrawContext(uint32 stringID) : StringID(stringID) { std::tie(String, Status) = G::Game.Text.Get(stringID); }
    };
    Controls::Table<uint32, decltype(FilteredList)&, DrawContext const&> Table { *this };

    StringListViewer(uint32 id, bool newTab) : Base(id, newTab)
    {
        Table.SetShowFilterRow();
        Table.AddColumn("ID",
        {
            .Width = 50,
            .Filter = [](uint32 id) { return id; },
            .Sort = [](decltype(FilteredList)& data, bool invert) { std::ranges::sort(data, [invert](auto a, auto b) { return a < b ^ invert; }); },
            .Draw = [](DrawContext const& context)
            {
                I::SetNextItemAllowOverlap();
                I::Selectable(std::format("{}", context.StringID).c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
                if (scoped::PopupContextItem())
                {
                    static uint64 decryptionKey = 0;
                    I::Text("Text: %s%s", GetStatusText(context.Status), context.String ? Utils::Encoding::ToUTF8(*context.String).c_str() : "");

                    Controls::CopyButton("ID", context.StringID);
                    I::SameLine();
                    Controls::CopyButton("DataLink", G::UI.MakeDataLink(0x03, 0x100 + context.StringID));
                    I::SameLine();
                    Controls::CopyButton("Text", context.String ? *context.String : L"", context.String);

                    if (I::InputScalar("Decryption Key", ImGuiDataType_U64, context.KeyInfo ? &context.KeyInfo->Key : &decryptionKey))
                    {
                        if (!context.KeyInfo)
                            G::Game.Encryption.AddTextKeyInfo(context.StringID, { .Key = std::exchange(decryptionKey, 0) });
                        G::Game.Text.WipeCache(context.StringID);
                    }

                    if (I::Button("Search for Content References"))
                        G::Windows::ContentSearch.SearchForSymbolValue("StringID", context.StringID);
                }
            },
        });
        Table.AddColumn("Text",
        {
            .Width = -1,
            .Filter = [](uint32 id) { auto [string, status] = G::Game.Text.Get(id); return string ? *string : L""; },
            .Draw = [](DrawContext const& context) { I::Text("%s%s", GetStatusText(context.Status), Utils::String::SingleLined(context.String ? Utils::Encoding::ToUTF8(*context.String).c_str() : "").c_str()); },
        });
        Table.AddColumn("Decrypted",
        {
            .Width = 80,
            .PreferSortDescending = true,
            .Filter = [](uint32 id) { auto info = G::Game.Encryption.GetTextKeyInfo(id); return info ? info->Encounter.LocalTime : Time::LocalPoint(); },
            .Sort = [](decltype(FilteredList)& data, bool invert)
            {
                Utils::Sort::ComplexSort(data, invert, [](uint32 id) { return G::Game.Encryption.GetTextKeyInfo(id); }, [](uint32 a, uint32 b, auto const& aInfo, auto const& bInfo)
                {
                    #define COMPARE(a, b) do { if (auto const result = (a) <=> (b); result != std::strong_ordering::equal) return result == std::strong_ordering::less; } while (false)
                    if (aInfo && bInfo)
                    {
                        COMPARE(aInfo->Encounter.Time, bInfo->Encounter.Time);
                        COMPARE(aInfo->Index, bInfo->Index);
                    }
                    else
                        COMPARE((bool)aInfo, (bool)bInfo);
                    COMPARE(a, b);
                    return false;
                    #undef COMPARE
                });
            },
            .Draw = [](DrawContext const& context)
            {
                if (context.KeyInfo)
                    Controls::Encounter(context.KeyInfo->Encounter, { .TimeText = "Decrypted on" });
            },
        });
        Table.AddColumn("Voice",
        {
            .Width = 80,
            .Filter = [](uint32 id) -> std::decay_t<decltype(*G::Game.Text.GetVariants(id))>
            {
                if (auto const variants = G::Game.Text.GetVariants(id))
                    return *variants;
                if (auto const voice = G::Game.Text.GetVoice(id))
                    return { voice };
                return { };
            },
            .Sort = [](decltype(FilteredList)& data, bool invert)
            {
                Utils::Sort::ComplexSort(data, invert, [](uint32 id) -> uint32
                {
                    if (auto const variants = G::Game.Text.GetVariants(id))
                        return variants->front();
                    if (auto const voice = G::Game.Text.GetVoice(id))
                        return voice;
                    return { };
                });
            },
            .Draw = [](DrawContext const& context){ Controls::TextVoiceButton(context.StringID, { .Selectable = true }); },
        });

        UpdateSearch();
    }

    void SetResult(Utils::Async::Context context, std::vector<uint32>&& data)
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

        AsyncFilter.Run([this, table = Table.Prepare()](Utils::Async::Context context)
        {
            context->SetIndeterminate();
            std::vector<uint32> data;
            {
                std::shared_lock _(Lock);
                data = FilteredList;
            }
            CHECK_ASYNC;
            table.Sort(data);
            SetResult(context, std::move(data));
        });
    }
    void UpdateSearch() override
    {
        bool textSearch = false;
        FilterID.reset();
        if (FilterString.empty())
            ;
        else if (uint32 id, range; Utils::Scan::Into(FilterString, "{}+{}", id, range))
            FilterID.emplace(id - range, id + range);
        else if (Utils::Scan::Into(FilterString, "{}-{}", id, range))
            FilterID.emplace(id, range);
        else if (Utils::Scan::NumberLiteral(FilterString, id))
            FilterID.emplace(id - FilterRange, id + FilterRange);
        else if (Utils::Scan::NumberLiteral(FilterString, "0x{:x}", id))
            FilterID.emplace(id - FilterRange, id + FilterRange);
        else
            textSearch = true;

        AsyncFilter.Run([this, textSearch, filter = FilterID, string = FilterString, filters = Filters, table = Table.Prepare()](Utils::Async::Context context) mutable
        {
            context->SetIndeterminate();
            Utils::Thread::SleepUntil(100ms, [] { return G::Game.Text.IsLoaded(G::Config.Language); });
            auto limits = filter.value_or(std::pair { std::numeric_limits<int32>::min(), std::numeric_limits<int32>::max() });
            limits.first = std::max(0, limits.first);
            limits.second = std::min((int32)G::Game.Text.GetMaxID() - 1, limits.second);
            std::vector<uint32> data;
            data.resize(limits.second - limits.first + 1);
            std::ranges::iota(data, limits.first);
            CHECK_ASYNC;
            if (!(filters.Unencrypted && filters.Encrypted && filters.Decrypted && filters.Empty))
            {
                std::erase_if(data, [&filters](uint32 id)
                {
                    auto [string, status] = G::Game.Text.Get(id);
                    if (!filters.Empty && string && (string->empty() || *string == L"[null]"))
                        return true;
                    switch (status)
                    {
                        using enum Data::Encryption::Status;
                        case Unencrypted: return !filters.Unencrypted;
                        case Encrypted: return !filters.Encrypted;
                        case Decrypted: return !filters.Decrypted;
                        case Missing: default: std::terminate();
                    }
                });
            }
            CHECK_ASYNC;
            if (table.HasFilterTerms() || textSearch)
            {
                static std::mutex parallelLock;
                std::unique_lock _(parallelLock);
                static std::unordered_map<std::thread::id, std::vector<uint32>> parallelResults;
                static std::mutex lock;
                std::ranges::for_each(parallelResults | std::views::values, &std::vector<uint32>::clear);
                std::for_each(std::execution::par_unseq, data.begin(), data.end(), [&table, textSearch, query = Utils::String::Uppercased(Utils::Encoding::FromUTF8(string))](uint32 stringID)
                {
                    thread_local auto& results = []() -> auto&
                    {
                        std::scoped_lock _(lock);
                        auto& container = parallelResults[std::this_thread::get_id()];
                        container.reserve(10000);
                        return container;
                    }();

                    if (!table.Filter(stringID))
                        return;

                    if (textSearch)
                        if (auto const& [string, status] = G::Game.Text.GetNormalized(stringID); !string || string->empty() || !string->contains(query))
                            return;

                    results.emplace_back(stringID);
                });
                CHECK_ASYNC;
                data.assign_range(std::views::join(parallelResults | std::views::values));
            }
            CHECK_ASYNC;
            table.Sort(data);
            SetResult(context, std::move(data));
        });
    }

    void Draw() override
    {
        if (scoped::TableDockRight("Search"))
        {
            I::TableNextColumn();
            if (static bool focus = true; std::exchange(focus, false))
                I::SetKeyboardFocusHere();
            if (Controls::SearchInput(FilterString, FilteredList, Lock, &AsyncFilter))
                UpdateSearch();

            I::TableNextColumn();
            static bool trackClipboard = false;
            static auto trackClipboardCooldown = Time::FrameStart;
            static std::string previousClipboardContents;
            if (I::CheckboxButton(ICON_FA_CLIPBOARD, trackClipboard, "Track Clipboard", I::GetFrameHeight()) && trackClipboard)
                previousClipboardContents = I::GetClipboardText();
            if (trackClipboard && Time::FrameStart >= trackClipboardCooldown)
            {
                trackClipboardCooldown = Time::FrameStart + 100ms;
                if (auto clipboard = I::GetClipboardText(); clipboard && previousClipboardContents != clipboard)
                {
                    FilterString = previousClipboardContents = clipboard;
                    UpdateSearch();
                }
            }
            I::SameLine(0, 0);
            static bool copySingleResult = false;
            std::string singleResult;
            if (std::shared_lock __(Lock); FilteredList.size() == 1)
                if (auto [string, status] = G::Game.Text.Get(FilteredList.front()); string)
                    singleResult = Utils::Encoding::ToUTF8(*string);
            static std::string previousSingleResult;
            if (I::CheckboxButton(ICON_FA_COPY, copySingleResult, "Auto Copy Single Result", I::GetFrameHeight()) && copySingleResult && !singleResult.empty())
                previousSingleResult = singleResult;
            if (copySingleResult && !singleResult.empty() && previousSingleResult != singleResult)
                I::SetClipboardText((previousClipboardContents = previousSingleResult = singleResult).c_str());
            I::SameLine();
            if (Controls::SearchFilterRange(FilterID, FilterRange))
                UpdateSearch();
        }

        auto filter = [&, next = false](std::string_view text, bool& filter) mutable
        {
            if (std::exchange(next, true))
                I::SameLine();
            if (I::Button(std::format("<c=#{}><c=#8>{}</c> {}</c>###StringFilter{}", filter ? "F" : "4", ICON_FA_FILTER, text, text).c_str()))
            {
                filter ^= true;
                UpdateSearch();
            }
        };
        filter("Unencrypted", Filters.Unencrypted);
        filter("Encrypted", Filters.Encrypted);
        filter("Decrypted", Filters.Decrypted);
        filter("Empty", Filters.Empty);

        if (scoped::TableList("Table", Table.GetColumnCount()))
        {
            Table.DrawColumnHeaders();

            std::shared_lock __(Lock);
            ImGuiListClipper clipper;
            clipper.Begin(FilteredList.size());
            while (clipper.Step())
                for (auto stringID : std::span(FilteredList.begin() + clipper.DisplayStart, FilteredList.begin() + clipper.DisplayEnd))
                    if (scoped::WithID(stringID))
                        Table.DrawRow(stringID);
        }
    }
};

}
