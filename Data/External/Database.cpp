module;
#include <concurrentqueue/blockingconcurrentqueue.h>

module GW2Viewer.Data.External.Database;
import GW2Viewer.Content.Conversation;
import GW2Viewer.Content.Event;
import GW2Viewer.Content.MapCinematic;
import GW2Viewer.Content.MysticForgeEntry;
import GW2Viewer.Content.Vendor;
import GW2Viewer.Data.Encryption.Asset;
import GW2Viewer.Data.Game;
import GW2Viewer.UI.Manager;
import GW2Viewer.UI.Viewers.ConversationListViewer;
import GW2Viewer.UI.Viewers.EventListViewer;
import GW2Viewer.UI.Viewers.ListViewer;
import GW2Viewer.UI.Viewers.MapCinematicListViewer;
import GW2Viewer.UI.Viewers.MysticForgeEntryListViewer;
import GW2Viewer.UI.Viewers.StringListViewer;
import GW2Viewer.UI.Viewers.VendorListViewer;
import <cassert>;

namespace GW2Viewer::Data::External
{

void OperationBase::PostProcessImpl()
{
    if (Options.PostHandler)
        G::UI.Defer(std::function(Options.PostHandler));
}

std::shared_ptr<OperationBase> Database::AddCommand(std::shared_ptr<OperationBase> command)
{
    CommandQueue.enqueue(command);
    return command;
}

void Database::CommandConnection::OnLoop()
{
    std::shared_ptr<OperationBase> operation;
    if (G::Database.CommandQueue.wait_dequeue_timed(operation, 50ms) && !operation->Cancelled)
    {
        Utils::Async::ProgressBarContext().ShowNotification().Run([this, operation](Utils::Async::ProgressBarContext& progress)
        {
            progress.Start(std::format("Loading data from {}", operation->Table));
            operation->Process(*Database, &progress, 50ms);
        }); // blocks until completed
    }
}

void Database::CreateLoadOperations()
{
    namespace Content = GW2Viewer::Content;

    auto updateConversationSearch = [] { G::Viewers::Notify(&UI::Viewers::ConversationListViewer::UpdateSearch); };
    auto updateEventFilter = [] { G::Viewers::Notify(&UI::Viewers::EventListViewer::UpdateSearch); };
    auto updateVendorSearch = [] { G::Viewers::Notify(&UI::Viewers::VendorListViewer::UpdateSearch); };
    auto updateMapCinematicSearch = [] { G::Viewers::Notify(&UI::Viewers::MapCinematicListViewer::UpdateSearch); };

    Load("Texts",
        "TextID, Key, Time, Session, Map, ClientX, ClientY, ClientZ, ClientFacing",
        [](uint32 TextID, uint64 Key, uint32 Time, uint32 Session, std::optional<uint32> Map, std::optional<float> ClientX, std::optional<float> ClientY, std::optional<float> ClientZ, std::optional<float> ClientFacing)
        {
            if (Map)
                G::Game.Encryption.AddTextKeyInfo(TextID, { Key, { (uint64)Time * 1000, { *ClientX, *ClientY, *ClientZ, *ClientFacing }, *Map, Session } });
            else
                G::Game.Encryption.AddTextKeyInfo(TextID, { Key, { (uint64)Time * 1000, Session } });
        },
        {
            .SharedMutex = &G::Game.Encryption.Mutex(),
            .PostHandler = [=]
            {
                G::Viewers::ForEach<UI::Viewers::StringListViewer>([](UI::Viewers::StringListViewer& viewer)
                {
                    if (!viewer.FilterString.empty() && !viewer.FilterID || viewer.FilteredList.empty())
                        viewer.UpdateSearch();
                    else if (viewer.Sort == UI::Viewers::StringListViewer::StringSort::Text || viewer.Sort == UI::Viewers::StringListViewer::StringSort::DecryptionTime)
                        viewer.UpdateSort();
                });
            },
        });
    Load("Assets",
        "AssetType, AssetID, Key",
        [](Encryption::AssetType AssetType, uint32 AssetID, uint64 key)
        {
            if (key)
                G::Game.Encryption.AddAssetKey(AssetType, AssetID, key);
            if (AssetType == Encryption::AssetType::Voice)
                G::Game.Voice.WipeCache(AssetID);
        },
        {
            .SharedMutex = &G::Game.Encryption.Mutex(),
        });
    Load("Conversations",
        "GenID, UID, FirstEncounteredTime, LastEncounteredTime",
        [](uint32 GenID, uint32 UID, uint32 FirstEncounteredTime, uint32 LastEncounteredTime)
        {
            Content::conversations[GenID].UID = UID;
        },
        {
            .SharedMutex = &Content::conversationsLock,
            .PostHandler = updateConversationSearch,
        });
    Load("ConversationStates",
        "GenID, StateID, TextID, SpeakerNameTextID, SpeakerPortraitOverrideFileID, Priority, Flags, Voting, Timeout, CostAmount, CostType, Unk",
        [](uint32 GenID, uint32 StateID, uint32 TextID, uint32 SpeakerNameTextID, uint32 SpeakerPortraitOverrideFileID, uint32 Priority, uint32 Flags, uint32 Voting, uint32 Timeout, uint32 CostAmount, uint32 CostType, uint32 Unk)
        {
            Content::conversations[GenID].States.emplace(StateID, TextID, SpeakerNameTextID, SpeakerPortraitOverrideFileID, Priority, Flags, Voting, Timeout, CostAmount, CostType, Unk);
        },
        {
            .SharedMutex = &Content::conversationsLock,
            .PostHandler = updateConversationSearch,
        });
    Load("ConversationStateTransitions",
        "GenID, StateID, StateTextID, TransitionID, TextID, CostAmount, CostType, CostKarma, Diplomacy, Unk, Personality, Icon, SkillDefDataID",
        [](uint32 GenID, uint32 StateID, uint32 StateTextID, int32 TransitionID, uint32 TextID, uint32 CostAmount, uint32 CostType, uint32 CostKarma, uint32 Diplomacy, uint32 Unk, uint32 Personality, uint32 Icon, uint32 SkillDefDataID)
        {
            for (auto& state : Content::conversations[GenID].States | std::views::filter([StateID, StateTextID](auto const& state) { return state.StateID == StateID && state.TextID == StateTextID; }))
                state.Transitions.emplace(TransitionID, TextID, CostAmount, CostType, CostKarma, Diplomacy, Unk, Personality, Icon, SkillDefDataID);
        },
        {
            .SharedMutex = &Content::conversationsLock,
            .PostHandler = updateConversationSearch,
        });
    Load("ConversationStateTransitionTargets",
        "GenID, StateID, StateTextID, TransitionID, TransitionTextID, TargetStateID, Flags",
        [](uint32 GenID, uint32 StateID, uint32 StateTextID, int32 TransitionID, uint32 TransitionTextID, int32 TargetStateID, uint32 Flags)
        {
            for (auto& state : Content::conversations[GenID].States | std::views::filter([StateID, StateTextID](auto const& state) { return state.StateID == StateID && state.TextID == StateTextID; }))
                for (auto& transition : state.Transitions | std::views::filter([TransitionID, TransitionTextID](auto const& transition) { return transition.TransitionID == TransitionID && transition.TextID == TransitionTextID; }))
                    transition.Targets.emplace(TargetStateID, Flags);
        },
        {
            .SharedMutex = &Content::conversationsLock,
            .PostHandler = updateConversationSearch,
        });
    Load("AgentConversation",
        "Time, MapSession, AgentX, AgentY, AgentZ, AgentFacing, ConversationGenID, ConversationStateID, ConversationStateTextID, ConversationStateTransitionID, ConversationStateTransitionTextID, Session, Map",
        [](uint64 Time, uint32 MapSession, float AgentX, float AgentY, float AgentZ, float AgentFacing, uint32 ConversationGenID, uint32 ConversationStateID, uint32 ConversationStateTextID, int32 ConversationStateTransitionID, int32 ConversationStateTransitionTextID, uint32 Session, uint32 Map)
        {
            auto& conversation = Content::conversations[ConversationGenID];
            conversation.Encounter = { Time, { AgentX, AgentY, AgentZ, AgentFacing }, Map, MapSession, Session };

            for (auto& state : conversation.States | std::views::filter([ConversationStateID, ConversationStateTextID](auto const& state) { return state.StateID == ConversationStateID && state.TextID == ConversationStateTextID; }))
            {
                state.Encounter = conversation.Encounter;

                if (ConversationStateTransitionTextID != -1)
                    for (auto& transition : state.Transitions | std::views::filter([ConversationStateTransitionID, ConversationStateTransitionTextID](auto const& transition) { return transition.TransitionID == ConversationStateTransitionID && transition.TextID == ConversationStateTransitionTextID; }))
                        transition.Encounter = conversation.Encounter;
            }
        },
        {
            .SharedMutex = &Content::conversationsLock,
            .PostHandler = updateConversationSearch,
        });
    Load("Vendors",
        "Hash, NameTextID, IconFileID, Time",
        [](uint64 Hash, uint32 NameTextID, uint32 IconFileID, uint64 Time)
        {
            auto& vendor = Content::vendors[Hash];
            vendor.NameTextID = NameTextID;
            vendor.IconFileID = IconFileID;
            vendor.Encounter = Time;
        },
        {
            .SharedMutex = &Content::vendorsLock,
            .PostHandler = updateVendorSearch,
        });
    Load("VendorServiceTabs",
        "Hash, ServiceType, TabIndex, CurrencyType, CurrencyDefDataID, ItemDefDataID, CurrencyTypeSecondary, CurrencyDefSecondaryDataID, ItemDefSecondaryDataID, IconFileID, NameTextID, Type, Time",
        [](uint64 Hash, uint32 ServiceType, uint32 TabIndex, uint32 CurrencyType, uint32 CurrencyDefDataID, uint32 ItemDefDataID, uint32 CurrencyTypeSecondary, uint32 CurrencyDefSecondaryDataID, uint32 ItemDefSecondaryDataID, uint32 IconFileID, uint32 NameTextID, uint32 Type, uint64 Time)
        {
            Content::vendors[Hash].ServiceTabs.emplace(ServiceType, TabIndex, CurrencyType, CurrencyDefDataID, ItemDefDataID, CurrencyTypeSecondary, CurrencyDefSecondaryDataID, ItemDefSecondaryDataID, IconFileID, NameTextID, Type);
        },
        {
            .SharedMutex = &Content::vendorsLock,
            .PostHandler = updateVendorSearch,
        });
    Load("VendorInventoryItems",
        "Hash, ServiceType, TabIndex, Slot, ItemDefDataID, BuyQuantity, Flags, State, UnlockTextID, A, B, TabLimit, TabLimitScope, TabLimitLifetime, ProgressDefDataID, CostDetails, Time",
        [](uint64 Hash, uint32 ServiceType, uint32 TabIndex, uint32 Slot, uint32 ItemDefDataID, uint32 BuyQuantity, uint32 Flags, uint32 State, uint32 UnlockTextID, uint32 A, uint32 B, int32 TabLimit, uint32 TabLimitScope, uint32 TabLimitLifetime, uint32 ProgressDefDataID, std::vector<byte> const& CostDetails, uint64 Time)
        {
            for (auto& tab : Content::vendors[Hash].ServiceTabs | std::views::filter([ServiceType, TabIndex](auto const& tab) { return tab.ServiceType == ServiceType && tab.TabIndex == TabIndex; }))
            {
                using Currency = Content::Vendor::ServiceTab::InventoryItem::Currency;
                assert(!(CostDetails.size() % sizeof(Currency)));
                auto& item = *tab.InventoryItems.emplace(Slot, ItemDefDataID, BuyQuantity, Flags, State, UnlockTextID, A, B, TabLimit, TabLimitScope, TabLimitLifetime, ProgressDefDataID, std::vector((Currency const*)CostDetails.data(), (Currency const*)(CostDetails.data() + CostDetails.size()))).first;
                item.Encounter = Time;
            }
        },
        {
            .SharedMutex = &Content::vendorsLock,
            .PostHandler = updateVendorSearch,
        });
    Load("AgentVendor",
        "Time, MapSession, AgentID, AgentX, AgentY, AgentZ, AgentFacing, VendorHash, VendorServiceType, VendorTabIndex, Session, Map, ClientX, ClientY, ClientZ, ClientFacing",
        [](uint64 Time, uint32 MapSession, std::optional<uint32> AgentID, std::optional<float> AgentX, std::optional<float> AgentY, std::optional<float> AgentZ, std::optional<float> AgentFacing, uint64 VendorHash, uint32 VendorServiceType, uint32 VendorTabIndex, uint32 Session, uint32 Map, float ClientX, float ClientY, float ClientZ, float ClientFacing)
        {
            auto& vendor = Content::vendors[VendorHash];
            vendor.Encounter = { Time, { AgentX.value_or(ClientX), AgentY.value_or(ClientY), AgentZ.value_or(ClientZ), AgentFacing.value_or(ClientFacing) }, Map, MapSession, Session };

            for (auto& tab : vendor.ServiceTabs | std::views::filter([VendorHash, VendorServiceType, VendorTabIndex](auto const& tab) { return tab.ServiceType == VendorServiceType && tab.TabIndex == VendorTabIndex; }))
                tab.Encounter = vendor.Encounter;
        },
        {
            .SharedMutex = &Content::vendorsLock,
            .PostHandler = updateVendorSearch,
        });
    Load("MapCinematics",
        "Hash, UID, Flags, Groups, Objects, Sectors, Time",
        [](uint64 Hash, uint32 UID, uint32 Flags, std::vector<byte> const& Groups, std::vector<byte> const& Objects, std::vector<byte> const& Sectors, uint64 Time)
        {
            using Group = Content::MapCinematic::Group;
            using Object = Content::MapCinematic::Object;
            using Sector = Content::MapCinematic::Sector;
            assert(!(Groups.size() % sizeof(Group)));
            assert(!(Objects.size() % sizeof(Object)));
            assert(!(Sectors.size() % sizeof(Sector)));
            auto& mapCinematic = Content::mapCinematics[Hash];
            mapCinematic.UID = UID;
            mapCinematic.Flags = Flags;
            mapCinematic.Groups = { (Group const*)Groups.data(), (Group const*)(Groups.data() + Groups.size()) };
            mapCinematic.Objects = { (Object const*)Objects.data(), (Object const*)(Objects.data() + Objects.size()) };
            mapCinematic.Sectors = { (Sector const*)Sectors.data(), (Sector const*)(Sectors.data() + Sectors.size()) };
            mapCinematic.Encounter = Time;
        },
        {
            .SharedMutex = &Content::mapCinematicsLock,
            .PostHandler = updateMapCinematicSearch,
        });
    Load("AgentMapCinematic",
        "Time, MapSession, AgentID, AgentX, AgentY, AgentZ, AgentFacing, MapCinematicHash, Session, Map, ClientX, ClientY, ClientZ, ClientFacing",
        [](uint64 Time, uint32 MapSession, uint32 AgentID, float AgentX, float AgentY, float AgentZ, float AgentFacing, uint64 MapCinematicHash, uint32 Session, uint32 Map, float ClientX, float ClientY, float ClientZ, float ClientFacing)
        {
            auto& mapCinematic = Content::mapCinematics[MapCinematicHash];
            mapCinematic.Encounter = { Time, { AgentX, AgentY, AgentZ, AgentFacing }, Map, MapSession, Session };
        },
        {
            .SharedMutex = &Content::mapCinematicsLock,
            .PostHandler = updateMapCinematicSearch,
        });
    Load("MysticForgeEntries",
        "GameBuild, UID, GameMode, Ingredients, Time",
        [](uint32 GameBuild, uint32 UID, uint32 GameMode, std::vector<byte> const& Ingredients, uint64 Time)
        {
            assert(Ingredients.size() == sizeof(Content::MysticForgeEntry::Ingredients));
            auto& mysticForgeEntry = Content::mysticForgeEntries[{ GameBuild, UID }];
            mysticForgeEntry.GameMode = GameMode;
            for (auto&& [index, stored] : mysticForgeEntry.Ingredients | std::views::enumerate)
                if (stored.IngredientType == Content::MYSTIC_FORGE_INGREDIENT_TYPES)
                    stored = ((struct Content::MysticForgeEntry::Ingredient const*)Ingredients.data())[index];
            mysticForgeEntry.Encounter = Time;
        },
        {
            .SharedMutex = &Content::mysticForgeEntriesLock,
            .PostHandler = [] { G::Viewers::Notify(&UI::Viewers::MysticForgeEntryListViewer::UpdateSearch); },
        });
    Load("Events",
        "Map, UID, TitleTextID, TitleParameterTextID1, TitleParameterTextID2, TitleParameterTextID3, TitleParameterTextID4, TitleParameterTextID5, TitleParameterTextID6, DescriptionTextID, FileIconID, FlagsClient, FlagsServer, Level, MetaTextTextID, AudioEffect, A, Time",
        [](uint32 Map, uint32 UID, uint32 TitleTextID, uint32 TitleParameterTextID1, uint32 TitleParameterTextID2, uint32 TitleParameterTextID3, uint32 TitleParameterTextID4, uint32 TitleParameterTextID5, uint32 TitleParameterTextID6, uint32 DescriptionTextID, uint32 FileIconID, Content::Event::State::ClientFlags FlagsClient, Content::Event::State::ServerFlags FlagsServer, uint32 Level, uint32 MetaTextTextID, GUID const& AudioEffect, uint32 A, uint64 Time)
        {
            Content::events[{ Map, UID }].States.emplace(Map, UID, TitleTextID, std::array { TitleParameterTextID1, TitleParameterTextID2, TitleParameterTextID3, TitleParameterTextID4, TitleParameterTextID5, TitleParameterTextID6 }, DescriptionTextID, FileIconID, FlagsClient, FlagsServer, Level, MetaTextTextID, AudioEffect, A).first->Encounter = Time;
        },
        {
            .SharedMutex = &Content::eventsLock,
            .PostHandler = updateEventFilter,
        });
    Load("Objectives",
        "Map, EventUID, EventObjectiveIndex, Type, Flags, TargetCount, TextID, AgentNameTextID, ProgressBarStyle, ExtraInt, ExtraInt2, ExtraGUID, ExtraGUID2, ExtraBlob, Time",
        [](uint32 Map, uint32 EventUID, uint32 EventObjectiveIndex, uint32 Type, uint32 Flags, uint32 TargetCount, uint32 TextID, uint32 AgentNameTextID, GUID const& ProgressBarStyle, uint32 ExtraInt, uint32 ExtraInt2, GUID const& ExtraGUID, GUID const& ExtraGUID2, std::vector<byte> const& ExtraBlob, uint64 Time)
        {
            Content::events[{ Map, EventUID }].Objectives.emplace(Map, EventUID, EventObjectiveIndex, Type, Flags, TargetCount, TextID, AgentNameTextID, ProgressBarStyle, ExtraInt, ExtraInt2, ExtraGUID, ExtraGUID2, ExtraBlob).first->Encounter = Time;
        },
        {
            .SharedMutex = &Content::eventsLock,
            .PostHandler = updateEventFilter,
        });
    Load("ObjectiveAgents",
        "ObjectiveMap, ObjectiveEventUID, ObjectiveEventObjectiveIndex, ObjectiveAgentIndex, ObjectiveAgentID, IFNULL(NULLIF(ObjectiveAgentNameTextID, 0), NameTextID), ObjectiveAgentX, ObjectiveAgentY, ObjectiveAgentZ, ObjectiveAgentFacing",
        [](uint32 ObjectiveMap, uint32 ObjectiveEventUID, uint32 ObjectiveEventObjectiveIndex, uint32 ObjectiveAgentIndex, uint32 ObjectiveAgentID, uint32 ObjectiveAgentNameTextID, float ObjectiveAgentX, float ObjectiveAgentY, float ObjectiveAgentZ, float ObjectiveAgentFacing)
        {
            for (auto& objective : Content::events[{ ObjectiveMap, ObjectiveEventUID }].Objectives | std::views::filter([ObjectiveEventObjectiveIndex](auto const& objective) { return objective.EventObjectiveIndex == ObjectiveEventObjectiveIndex; }))
            {
                objective.Agents.resize(std::max<size_t>(objective.Agents.size(), ObjectiveAgentIndex + 1));
                objective.Agents.at(ObjectiveAgentIndex) = { ObjectiveAgentID, ObjectiveAgentNameTextID };
            }
        },
        {
            .Joins = "LEFT JOIN Agents a ON ObjectiveAgents.Session=a.Session AND ObjectiveAgents.MapSession=a.MapSession AND ObjectiveAgents.ObjectiveMap=a.Map AND ObjectiveAgents.ObjectiveAgentID=a.AgentID",
            .SharedMutex = &Content::eventsLock,
            .PostHandler = updateEventFilter,
        });
}

}
