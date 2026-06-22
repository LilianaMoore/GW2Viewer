export module GW2Viewer.Data.External.Types;
import GW2Viewer.Common;
import GW2Viewer.Common.Time;
import GW2Viewer.UI.ImGui;
import std;

export namespace GW2Viewer::Data::External
{

template<typename T>
struct DBBlobElementSpan
{
    uint16 StartIndex;
    uint16 Count;

    std::span<T const> GetSpan(std::span<T const> full) const
    {
        return full.subspan(StartIndex, Count);
    }
};
static_assert(sizeof(DBBlobElementSpan<void>) == 4);

struct Encounter
{
    Time::Point Time;
    Time::LocalPoint LocalTime = Time::ToLocal(Time);
    std::optional<ImVec4> Position;
    std::optional<uint32> Map;
    std::optional<uint32> MapSession;
    std::optional<uint32> Session;

    Encounter() { }
    Encounter(Time::Point time) : Time(time) { }
    Encounter(uint64 timestampMs) : Encounter(Time::FromTimestampMs(timestampMs)) { }
    Encounter(Time::Point time, ImVec4 position, uint32 map, uint32 mapSession, uint32 session) : Time(time), Position(position), Map(map), MapSession(mapSession), Session(session) { }
    Encounter(uint64 timestampMs, ImVec4 position, uint32 map, uint32 mapSession, uint32 session) : Time(Time::FromTimestampMs(timestampMs)), Position(position), Map(map), MapSession(mapSession), Session(session) { }
    Encounter(uint64 timestampMs, ImVec4 position, uint32 map, uint32 session) : Time(Time::FromTimestampMs(timestampMs)), Position(position), Map(map), Session(session) { }
    Encounter(uint64 timestampMs, uint32 session) : Time(Time::FromTimestampMs(timestampMs)), Session(session) { }
};

}
