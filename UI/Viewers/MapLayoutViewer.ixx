export module GW2Viewer.UI.Viewers.MapLayoutViewer;
import GW2Viewer.Common.GUID;
import GW2Viewer.Data.Game;
import GW2Viewer.Data.Map.DisplaySet;
import GW2Viewer.UI.Controls;
import GW2Viewer.UI.Viewers.ViewerRegistry;
import GW2Viewer.UI.Viewers.ViewerWithHistory;
import GW2Viewer.Utils.Visitor;
import std;
#include "Macros.h"

export namespace GW2Viewer::UI::Viewers
{

struct MapLayoutViewer : ViewerWithHistory<MapLayoutViewer, Data::Map::DisplaySet, { ICON_FA_GLOBE " World Map", "World Map", Category::Uncategorized }>
{
    TargetType DisplaySet;
    Controls::MapLayout MapLayout;

    MapLayoutViewer(uint32 id, bool newTab) : MapLayoutViewer(id, newTab, { }) { }
    MapLayoutViewer(uint32 id, bool newTab, HistoryType const& displaySet) : ViewerWithHistory(id, newTab), DisplaySet(displaySet)
    {
        for (auto const& element : DisplaySet.Elements)
        {
            std::visit(Utils::Visitor::Overloaded
            {
                [this](Data::Map::DisplaySet::ContinentFloor const& target)
                {
                    MapLayout.MapLayoutContinent = &target.MapLayoutContinent;
                    MapLayout.MapLayoutContinentFloor = &target.MapLayoutContinentFloor;
                },
                [this](Data::Map::DisplaySet::ContinentCoords const& target)
                {
                    auto&& [icon, _] = MapLayout.AddIcon(102538, { target.Position.x, target.Position.y }, 32);
                    icon.Centered = true;
                },
                [this](Data::Map::DisplaySet::MapCoords const& target)
                {
                    auto&& [icon, _] = MapLayout.AddIcon(102538, *target.MapDef, { target.PositionFacing.x, target.PositionFacing.y }, 32);
                    icon.Centered = true;
                },
            }, element);
        }

        if (!MapLayout.MapLayoutContinent)
            MapLayout.MapLayoutContinent = G::Game.Content.GetByGUID("21742531-35A3-4763-A670-8B774B2B27AC");
        if (!MapLayout.MapLayoutContinentFloor)
            MapLayout.MapLayoutContinentFloor = G::Game.Content.GetByGUID("DD8189AD-0359-4C17-AD7B-5CA5B33E52F3");

        MapLayout.Initialize();
    }

    TargetType GetCurrent() const override { return DisplaySet; }
    bool IsCurrent(TargetType target) const override { return false; }

    std::string Title() override { return ViewerInfo.Title; }
    void Draw() override
    {
        MapLayout.Draw([this]
        {
            DrawHistoryButtons();
        });
    }
};

}
