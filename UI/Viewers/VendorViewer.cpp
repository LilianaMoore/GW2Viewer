module GW2Viewer.UI.Viewers.VendorViewer;
import GW2Viewer.Content;
import GW2Viewer.Content.Vendor;
import GW2Viewer.Data.Game;
import GW2Viewer.Data.Text.Format;
import GW2Viewer.UI.Controls;
import GW2Viewer.UI.Manager;
import GW2Viewer.UI.ImGui;
import GW2Viewer.Utils.Encoding;
import std;
#include "Macros.h"

namespace GW2Viewer::UI::Viewers
{

std::string VendorViewer::Title()
{
    return std::format("<c=#4>{} </c>{}", Base::Title(), Content::vendors.at(VendorHash).Name());
}

void VendorViewer::Draw()
{
    static constexpr float vendorNameFontSize = 30;
    static constexpr float tabNameFontSize = 20;

    std::shared_lock _(Content::vendorsLock);
    auto const& vendor = Content::vendors.at(VendorHash);

    bool wikiWrite = false;
    std::string wikiBuffer;
    auto wiki = std::back_inserter(wikiBuffer);
    static bool verbose = false;
    if (scoped::Child(I::GetSharedScopeID("VendorViewer"), { }, ImGuiChildFlags_Borders | ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY))
    {
        DrawHistoryButtons();
        I::SameLine();
        if (I::Button(ICON_FA_COPY " Wiki Markup"))
        {
            wikiWrite = true;
            wikiBuffer.reserve(64 * 1024);
        }
        I::SameLine(); I::Checkbox("Verbose", &verbose);
    }

    if (scoped::Child("Contents"))
    {
        if (scoped::Font(G::UI.Fonts.GameHeading, vendorNameFontSize))
            I::TextUnformatted(vendor.Name().c_str());

        I::SameLine();
        if (scoped::RightAligned(&vendor.Encounter))
            Controls::Encounter(vendor.Encounter, { .Button = true });

        if (verbose)
        if (scoped::WithStyleVarY(ImGuiStyleVar_ItemSpacing, 0))
        {
            Controls::SymbolText("Name", vendor.NameTextID);
            Controls::SymbolFile("Icon", vendor.IconFileID);
        }

        for (auto const& tab : vendor.ServiceTabs)
        {
            bool open = true;
            if (scoped::WithStyleVarY(ImGuiStyleVar_FramePadding, I::GetStyle().FramePadding.y + (tabNameFontSize - I::GetTextLineHeight()) / 2)) // Hack to make arrow have the regular size while reserving space for text of bigger font size
                open = I::CollapsingHeader(std::format("##ServiceTab-{}-{}-{}-Header", VendorHash, tab.ServiceType, tab.TabIndex).c_str(), ImGuiTreeNodeFlags_AllowOverlap | (tab.ShouldHaveInventory() ? ImGuiTreeNodeFlags_DefaultOpen : 0));
            I::GetCurrentWindow()->DC.PrevLineTextBaseOffset = I::GetStyle().FramePadding.y; // Hack to restore cursor Y after the previous hack
            I::SameLine(0, 0);
            if (scoped::WithStyleVarY(ImGuiStyleVar_ItemSpacing, 0)) // Removes the spacing between the collapsible header and the inventory table
            if (scoped::Font(G::UI.Fonts.GameHeading, tabNameFontSize))
                I::TextUnformatted(tab.Name().c_str());

            if (scoped::WithCursorScreenPos(ImFloor(I::GetCurrentWindow()->DC.CursorPosPrevLine - ImVec2(0, (tabNameFontSize - I::GetTextLineHeight()) / 2))))
            if (scoped::RightAligned(&tab.Encounter))
                Controls::Encounter(tab.Encounter, { .Button = true });

            if (open)
            if (scoped::Child(std::format("##ServiceTab-{}-{}-{}", VendorHash, tab.ServiceType, tab.TabIndex).c_str(), { }, ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY))
            {
                if (verbose)
                if (scoped::WithStyleVarY(ImGuiStyleVar_ItemSpacing, 0))
                {
                    Controls::SymbolEnum("ServiceType", tab.ServiceType, "VendorServiceType");
                    Controls::Symbol("TabIndex", tab.TabIndex);
                    Controls::SymbolFile("Icon", tab.IconFileID);
                    Controls::SymbolText("Name", tab.NameTextID);
                    Controls::Symbol("Type", tab.Type);

                    auto drawTabCurrency = [](std::string_view name, uint32 const& type, uint32 const& currencyDefDataID, uint32 const& itemDefDataID)
                    {
                        Controls::SymbolEnum(name, type, "VendorCurrencyType");
                        if (auto const currencyDef = G::Game.Content.GetByDataID(Content::CurrencyDef, currencyDefDataID))
                        {
                            I::SameLine();
                            Controls::ContentButton(currencyDef, &currencyDefDataID);
                        }
                        if (auto const itemDef = G::Game.Content.GetByDataID(Content::ItemDef, itemDefDataID))
                        {
                            I::SameLine();
                            Controls::ContentButton(itemDef, &itemDefDataID);
                        }
                    };
                    drawTabCurrency("Currency 1", tab.CurrencyType, tab.CurrencyDefDataID, tab.ItemDefDataID);
                    drawTabCurrency("Currency 2", tab.CurrencyTypeSecondary, tab.CurrencyDefSecondaryDataID, tab.ItemDefSecondaryDataID);
                }

                if (tab.ShouldHaveInventory())
                {
                    if (wikiWrite)
                    {
                        auto [string, status] = G::Game.Text.Get(tab.NameTextID);
                        std::format_to(wiki, "=== {} ===\n{{{{Vendor table header}}}}\n", string ? Utils::Encoding::ToUTF8(*string).c_str() : "");
                    }

                    if (scoped::WithStyleVar(ImGuiStyleVar_CellPadding, ImVec2()))
                    if (scoped::WithStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2()))
                    if (scoped::Table("Inventory", 12, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_Hideable | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg))
                    {
                        auto const verboseColumn = verbose ? 0 : ImGuiTableColumnFlags_Disabled;
                        I::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30);
                        I::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthFixed, 50);
                        I::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
                        I::TableSetupColumn("Cost", ImGuiTableColumnFlags_WidthFixed, 150);
                        I::TableSetupColumn("Tab Limit", ImGuiTableColumnFlags_WidthFixed, 100);
                        I::TableSetupColumn("ProgressDef Limit", ImGuiTableColumnFlags_WidthFixed, 150);
                        I::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed | verboseColumn, 40);
                        I::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed | verboseColumn, 40);
                        I::TableSetupColumn("UnlockTextID", ImGuiTableColumnFlags_WidthFixed | verboseColumn, 100);
                        I::TableSetupColumn("A", ImGuiTableColumnFlags_WidthFixed | verboseColumn, 40);
                        I::TableSetupColumn("B", ImGuiTableColumnFlags_WidthFixed | verboseColumn, 40);
                        I::TableSetupColumn("Encounter", ImGuiTableColumnFlags_WidthFixed | verboseColumn, 100);
                        I::TableHeadersRow();

                        uint32 nextExpectedSlot = 0;
                        for (auto const& item : tab.InventoryItems)
                        {
                            for (auto missingSlot = nextExpectedSlot; missingSlot < item.Slot; ++missingSlot)
                            {
                                I::TableNextRow();

                                I::TableNextColumn();
                                I::AlignTextToFramePadding();
                                I::Text("  <c=#F00>%u</c>", missingSlot);
                                Controls::CompletionMargin(Content::Vendor::COMPLETENESS_INVENTORY_ITEM_MISSING);

                                I::TableNextColumn();

                                I::TableNextColumn();
                                I::AlignTextToFramePadding();
                                I::Text("<c=#F004>Inventory item missing</c>", missingSlot);
                            }
                            nextExpectedSlot = item.Slot + 1;

                            auto const itemDef = G::Game.Content.GetByDataID(Content::ItemDef, item.ItemDefDataID);
                            bool const wikiWriteItem = wikiWrite && itemDef;
                            if (wikiWriteItem)
                            {
                                std::format_to(wiki, "{{{{Vendor table row|item={}", Utils::Encoding::ToUTF8(Data::Text::FormatStringEmpty(itemDef->GetDisplayName(false, true, true))));
                                if (item.BuyQuantity != 1)
                                    std::format_to(wiki, "|quantity={}", item.BuyQuantity);
                            }

                            I::TableNextRow();

                            I::TableNextColumn();
                            I::AlignTextToFramePadding();
                            I::Text("  <c=#4>%u</c>", item.Slot);

                            I::TableNextColumn();
                            I::AlignTextToFramePadding();
                            I::RightAlignedText("{} <c=#4>x</c>", item.BuyQuantity);

                            I::TableNextColumn();
                            Controls::ContentButton(itemDef, &item.ItemDefDataID);

                            I::TableNextColumn();
                            if (scoped::RightAligned(&item.CostDetails))
                            {
                                for (auto const& [index, currency] : item.CostDetails | std::views::enumerate)
                                {
                                    if (index)
                                    {
                                        I::SameLine(0, 0);
                                        I::TextUnformatted("<c=#4>+</c>");
                                        I::SameLine(0, 0);
                                    }
                                    if (wikiWriteItem)
                                        wikiBuffer.append(index ? " + " : "|cost=");

                                    switch (currency.CurrencyType)
                                    {
                                        case Content::VENDOR_CURRENCY_TYPE_NONE:
                                            I::AlignTextToFramePadding();
                                            I::TextUnformatted("<c=#F00>VENDOR_CURRENCY_TYPE_NONE</c>");
                                            break;
                                        case Content::VENDOR_CURRENCY_TYPE_CURRENCY:
                                            if (currency.CurrencyDefDataID != 1)
                                            {
                                                auto const currencyDef = G::Game.Content.GetByDataID(Content::CurrencyDef, currency.CurrencyDefDataID);
                                                Controls::ContentButton(currencyDef, &currency.CurrencyDefDataID, { .Icon = std::format("{}", currency.Count), .HideContentType = true, .HideContentName = true });
                                                if (wikiWrite && currencyDef)
                                                    std::format_to(wiki, "{} {}", currency.Count, Utils::Encoding::ToUTF8(Data::Text::FormatStringEmpty(currencyDef->GetDisplayName(false, true, true))));
                                                break;
                                            }
                                            [[fallthrough]];
                                        case Content::VENDOR_CURRENCY_TYPE_COIN:
                                        {
                                            std::string text;
                                            if (uint32 const qty = currency.Count / 1'00'00)
                                                text += std::format("<c=#DB4>{}</c><img=156904/>", qty);
                                            if (uint32 const qty = currency.Count % 1'00'00 / 1'00)
                                                text += std::format("<c=#AAA>{}</c><img=156907/>", qty);
                                            if (uint32 const qty = currency.Count % 1'00)
                                                text += std::format("<c=#B62>{}</c><img=156902/>", qty);
                                            if (currency.CurrencyType == Content::VENDOR_CURRENCY_TYPE_CURRENCY)
                                            {
                                                auto const currencyDef = G::Game.Content.GetByDataID(Content::CurrencyDef, currency.CurrencyDefDataID);
                                                Controls::ContentButton(currencyDef, &currency.CurrencyDefDataID, { .Icon = text, .HideContentIcon = true, .HideContentType = true, .HideContentName = true });
                                            }
                                            else
                                            {
                                                I::AlignTextToFramePadding();
                                                I::TextUnformatted(text.c_str());
                                            }
                                            if (wikiWrite)
                                                std::format_to(wiki, "{}", currency.Count);
                                            break;
                                        }
                                        case Content::VENDOR_CURRENCY_TYPE_LAUREL:
                                        {
                                            I::AlignTextToFramePadding();
                                            I::TextUnformatted(std::format("<c=#4ADC46FF>{}</c><img=536029/>", currency.Count).c_str());
                                            if (wikiWrite)
                                                std::format_to(wiki, "{} laurel", currency.Count);
                                            break;
                                        }
                                        case Content::VENDOR_CURRENCY_TYPE_GLORY:
                                        {
                                            I::AlignTextToFramePadding();
                                            I::TextUnformatted(std::format("{}<img=155032/>", currency.Count).c_str());
                                            if (wikiWrite)
                                                std::format_to(wiki, "{} glory", currency.Count);
                                            break;
                                        }
                                        case Content::VENDOR_CURRENCY_TYPE_GUILD_FAVOR:
                                        {
                                            I::AlignTextToFramePadding();
                                            I::TextUnformatted(std::format("{}<img=1078523/>", currency.Count).c_str());
                                            if (wikiWrite)
                                                std::format_to(wiki, "{} favor", currency.Count);
                                            break;
                                        }
                                        case Content::VENDOR_CURRENCY_TYPE_GUILD_INFLUENCE:
                                        {
                                            I::AlignTextToFramePadding();
                                            I::TextUnformatted(std::format("{}<img=155033/>", currency.Count).c_str());
                                            if (wikiWrite)
                                                std::format_to(wiki, "{} influence", currency.Count);
                                            break;
                                        }
                                        case Content::VENDOR_CURRENCY_TYPE_ITEM:
                                        {
                                            auto const currencyItem = G::Game.Content.GetByDataID(Content::ItemDef, currency.ItemDefDataID);
                                            Controls::ContentButton(currencyItem, &currency.ItemDefDataID, { .Icon = std::format("{}", currency.Count), .HideContentType = true, .HideContentName = true });
                                            if (wikiWrite && currencyItem)
                                                std::format_to(wiki, "{} {}", currency.Count, Utils::Encoding::ToUTF8(Data::Text::FormatStringEmpty(currencyItem->GetDisplayName(false, true, true))));
                                            break;
                                        }
                                        case Content::VENDOR_CURRENCY_TYPE_KARMA:
                                        {
                                            I::AlignTextToFramePadding();
                                            I::TextUnformatted(std::format("<c=#D24DEBFF>{}</c><img=155026/>", currency.Count).c_str());
                                            if (wikiWrite)
                                                std::format_to(wiki, "{} karma", currency.Count);
                                            break;
                                        }
                                        case Content::VENDOR_CURRENCY_TYPE_SKILL_POINT:
                                        {
                                            I::AlignTextToFramePadding();
                                            I::TextUnformatted(std::format("{}<img=157173/>", currency.Count).c_str());
                                            if (wikiWrite)
                                                std::format_to(wiki, "{} skill point", currency.Count);
                                            break;
                                        }
                                        default:
                                            I::AlignTextToFramePadding();
                                            I::TextUnformatted("<c=#F00>Not implemented yet</c>");
                                            break;
                                    }
                                }
                            }

                            I::TableNextColumn();
                            if (item.TabLimit != -1)
                            {
                                I::AlignTextToFramePadding();
                                I::Text("%d", item.TabLimit);
                                switch (item.TabLimitScope)
                                {
                                    case 0:
                                        I::SameLine(0, 0);
                                        I::TextUnformatted(" / account");
                                        break;
                                    case 1:
                                        I::SameLine(0, 0);
                                        I::TextUnformatted(" / character");
                                        if (wikiWriteItem)
                                            std::format_to(wiki, "|per character={}", item.TabLimit);
                                        break;
                                    case 2:
                                        break;
                                    default:
                                        I::TextUnformatted("<c=#F00>Unhandled limit scope</c>");
                                        break;
                                }
                                switch (item.TabLimitLifetime)
                                {
                                    case 0:
                                        I::SameLine(0, 0);
                                        I::TextUnformatted(" / day");
                                        if (wikiWriteItem)
                                            std::format_to(wiki, "|per day={}", item.TabLimit);
                                        break;
                                    case 1:
                                        I::SameLine(0, 0);
                                        I::TextUnformatted(" / week");
                                        if (wikiWriteItem)
                                            std::format_to(wiki, "|per week={}", item.TabLimit);
                                        break;
                                    case 2:
                                        I::SameLine(0, 0);
                                        I::TextUnformatted(" / month");
                                        if (wikiWriteItem)
                                            std::format_to(wiki, "|per month={}", item.TabLimit);
                                        break;
                                    case 3:
                                        if (wikiWriteItem)
                                            std::format_to(wiki, "|total={}", item.TabLimit);
                                        break;
                                    case 4:
                                        break;
                                    default:
                                        I::TextUnformatted("<c=#F00>Unhandled limit lifetime</c>");
                                        break;
                                }
                            }

                            I::TableNextColumn();
                            if (auto const progressDef = G::Game.Content.GetByDataID(Content::ProgressDef, item.ProgressDefDataID))
                            {
                                Controls::ContentButton(progressDef, &item.ProgressDefDataID);
                                if (wikiWriteItem)
                                {
                                    auto const [bit, counter] = Data::Content::TypeInfo::FindEnumValues("ProgressDataType", "Bit", "Counter");
                                    uint32 count = 0;
                                    if (uint32 const dataType = (*progressDef)["DataType"]; dataType == bit)
                                        count = 1;
                                    else if (dataType == counter)
                                        count = (*progressDef)["Counter->ValueMax"];

                                    auto const [daily, monthly, weekly, permanent, accountDaily, accountWeekly, accountMonthly, accountPermanent] = Data::Content::TypeInfo::FindEnumValues("ProgressLifetimeType", "Daily", "Monthly", "Weekly", "Permanent", "AccountDaily", "AccountWeekly", "AccountMonthly", "AccountPermanent");
                                    if (uint32 const lifetime = (*progressDef)["Lifetime"]; lifetime == daily || lifetime == accountDaily)
                                        std::format_to(wiki, "|per day={}", count);
                                    else if (lifetime == weekly || lifetime == accountWeekly)
                                        std::format_to(wiki, "|per week={}", count);
                                    else if (lifetime == permanent)
                                        std::format_to(wiki, "|per character={}", count);
                                    else if (lifetime == accountPermanent)
                                        std::format_to(wiki, "|total={}", count);
                                }
                            }

                            I::TableNextColumn(); Controls::Value(item.Flags);
                            I::TableNextColumn(); Controls::Value(item.State);
                            I::TableNextColumn(); Controls::ValueText(item.UnlockTextID);
                            I::TableNextColumn(); Controls::Value(item.A);
                            I::TableNextColumn(); Controls::Value(item.B);
                            I::TableNextColumn(); Controls::Encounter(item.Encounter, { .Button = true });

                            if (wikiWriteItem)
                                wikiBuffer.append("}}\n");
                        }
                    }

                    if (wikiWrite)
                        wikiBuffer.append("|}\n");
                }
            }
        }
    }

    if (wikiWrite && !wikiBuffer.empty())
    {
        wikiBuffer.append(1, '\0');
        I::SetClipboardText(wikiBuffer.c_str());
    }
}

}
