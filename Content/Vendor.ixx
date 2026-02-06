export module GW2Viewer.Content.Vendor;
import GW2Viewer.Common;
import GW2Viewer.Common.Time;
import GW2Viewer.Data.External.Types;
import GW2Viewer.Data.Game;
import GW2Viewer.UI.ImGui;
import GW2Viewer.Utils.Encoding;
import std;

export namespace GW2Viewer::Content
{

enum EVendorCurrencyType : uint32
{
    VENDOR_CURRENCY_TYPE_NONE,
    VENDOR_CURRENCY_TYPE_COIN,
    VENDOR_CURRENCY_TYPE_LAUREL,
    VENDOR_CURRENCY_TYPE_CURRENCY,
    VENDOR_CURRENCY_TYPE_GLORY,
    VENDOR_CURRENCY_TYPE_GUILD_FAVOR,
    VENDOR_CURRENCY_TYPE_GUILD_INFLUENCE,
    VENDOR_CURRENCY_TYPE_ITEM,
    VENDOR_CURRENCY_TYPE_KARMA,
    VENDOR_CURRENCY_TYPE_SKILL_POINT,
};
enum EVendorServiceType : uint32
{
    VENDOR_SERVICE_TYPE_BUY,
    VENDOR_SERVICE_TYPE_BUYBACK,
    VENDOR_SERVICE_TYPE_DONATE,
    VENDOR_SERVICE_TYPE_KARMA,
    VENDOR_SERVICE_TYPE_REWARD,
    VENDOR_SERVICE_TYPE_SELL,
    VENDOR_SERVICE_TYPES,
};
struct Vendor
{
    enum Completeness
    {
        COMPLETENESS_INVENTORY_ITEM_MISSING,
        COMPLETENESS_COMPLETE,
    };

    struct ServiceTab
    {
        struct InventoryItem
        {
            struct Currency
            {
                uint32 CurrencyType;
                uint32 CurrencyDefDataID;
                uint32 ItemDefDataID;
                uint32 Count;

                auto GetIdentity() const { return std::tie(CurrencyType, CurrencyDefDataID, ItemDefDataID, Count); }
                auto operator<=>(Currency const& other) const { return GetIdentity() <=> other.GetIdentity(); }
            };

            uint32 Slot;

            uint32 ItemDefDataID;
            uint32 BuyQuantity;
            uint32 Flags;
            uint32 State;
            uint32 UnlockTextID;
            uint32 A;
            uint32 B;
            int32 TabLimit;
            uint32 TabLimitScope;
            uint32 TabLimitLifetime;
            uint32 ProgressDefDataID;
            std::vector<Currency> CostDetails;

            mutable Data::External::Encounter Encounter;

            auto GetIdentity() const { return std::tie(Slot, ItemDefDataID, BuyQuantity, Flags, State, UnlockTextID, A, B, TabLimit, TabLimitScope, TabLimitLifetime, ProgressDefDataID, CostDetails); }
            auto operator<=>(InventoryItem const& other) const { return GetIdentity() <=> other.GetIdentity(); }
        };

        uint32 ServiceType;
        uint32 TabIndex;

        uint32 CurrencyType;
        uint32 CurrencyDefDataID;
        uint32 ItemDefDataID;
        uint32 CurrencyTypeSecondary;
        uint32 CurrencyDefSecondaryDataID;
        uint32 ItemDefSecondaryDataID;
        uint32 IconFileID;
        uint32 NameTextID;
        uint32 Type;

        mutable std::set<InventoryItem> InventoryItems;
        mutable Data::External::Encounter Encounter;

        bool ShouldHaveInventory() const
        {
            switch (ServiceType)
            {
                case VENDOR_SERVICE_TYPE_BUY:
                case VENDOR_SERVICE_TYPE_KARMA:
                case VENDOR_SERVICE_TYPE_REWARD:
                    return true;
                case VENDOR_SERVICE_TYPE_BUYBACK:
                case VENDOR_SERVICE_TYPE_DONATE:
                case VENDOR_SERVICE_TYPE_SELL:
                    return false;
                default:
                    std::terminate();
            }
        }

        Completeness GetCompleteness() const
        {
            if (ShouldHaveInventory())
            {
                // No inventory recorded
                if (InventoryItems.empty())
                    return COMPLETENESS_INVENTORY_ITEM_MISSING;

                // Inventory starts not from the first slot
                if (InventoryItems.begin()->Slot)
                    return COMPLETENESS_INVENTORY_ITEM_MISSING;

                // Inventory has gaps between slots
                if (std::ranges::any_of(InventoryItems | std::views::pairwise, [](auto const& pair)
                {
                    auto const& [a, b] = pair;
                    return a.Slot != b.Slot && a.Slot + 1 != b.Slot;
                }))
                    return COMPLETENESS_INVENTORY_ITEM_MISSING;
            }
            return COMPLETENESS_COMPLETE;
        }

        auto GetIdentity() const { return std::tie(ServiceType, TabIndex, CurrencyType, CurrencyDefDataID, ItemDefDataID, CurrencyTypeSecondary, CurrencyDefSecondaryDataID, ItemDefSecondaryDataID, IconFileID, NameTextID, Type); }
        auto operator<=>(ServiceTab const& other) const { return GetIdentity() <=> other.GetIdentity(); }

        std::string Name() const
        {
            std::string result;
            if (auto string = G::Game.Text.Get(NameTextID).first)
                result = Utils::Encoding::ToUTF8(*string);
            uint32 icon = IconFileID;
            if (!icon)
            {
                switch (ServiceType)
                {
                    case VENDOR_SERVICE_TYPE_BUY:     icon = 156670; break;
                    case VENDOR_SERVICE_TYPE_BUYBACK: icon = 156749; break;
                    case VENDOR_SERVICE_TYPE_SELL:    icon = 156753; break;
                }
            }
            if (icon)
                result = std::format("<img={}/>{}", icon, result);
            return result;
        }
    };

    uint32 NameTextID;
    uint32 IconFileID;

    mutable std::set<ServiceTab> ServiceTabs;
    mutable Data::External::Encounter Encounter;

    Completeness GetCompleteness() const { return std::ranges::min(ServiceTabs | std::views::transform(&ServiceTab::GetCompleteness)); }

    std::string Name() const
    {
        std::string result;
        if (auto string = G::Game.Text.Get(NameTextID).first)
            result = Utils::Encoding::ToUTF8(*string);
        result = std::format("<img={}/>{}", IconFileID ? IconFileID : 156032, result);
        return result;
    }
    std::string TabNames() const
    {
        std::string result;
        for (auto const& tab : ServiceTabs)
            result = std::format("{}{}{}", result, result.empty() ? "" : " ", tab.Name());
        return result;
    }
};
std::map<uint64, Vendor> vendors;
std::shared_mutex vendorsLock;

}
