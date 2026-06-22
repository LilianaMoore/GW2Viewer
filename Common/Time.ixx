module;
#include "Utils/Scan.h"
#include <time.h>

export module GW2Viewer.Common.Time;
import GW2Viewer.Common;
import GW2Viewer.Utils.ConstString;
import GW2Viewer.Utils.Scan;
import std;
import <corecrt.h>;

export using namespace std::chrono_literals;

export namespace GW2Viewer::Time
{

using Us = std::chrono::microseconds;
using Ms = std::chrono::milliseconds;
using Secs = std::chrono::seconds;
using Mins = std::chrono::minutes;
using Hours = std::chrono::hours;
using Days = std::chrono::days;
using Weeks = std::chrono::weeks;
using Months = std::chrono::months;
using Years = std::chrono::years;

using Timestamp = time_t;
using TimestampMs = int64;

using Clock = std::chrono::system_clock;
using Point = Clock::time_point;
using PointMs = decltype(std::chrono::floor<Ms>(Point { }));
using PointSecs = decltype(std::chrono::floor<Secs>(Point { }));
using Duration = Clock::duration;

using LocalPoint = std::chrono::local_time<Duration>;

using FileClock = std::chrono::file_clock;
using FilePoint = FileClock::time_point;
using FileDuration = FileClock::duration;

using PreciseClock = std::chrono::high_resolution_clock;
using PrecisePoint = PreciseClock::time_point;
using PreciseDuration = PreciseClock::duration;

Point Now() { return Clock::now(); }
PrecisePoint PreciseNow() { return PreciseClock::now(); }

template<typename Dur, typename Clock, typename FromDur> std::chrono::time_point<Clock, Dur> Cast(std::chrono::time_point<Clock, FromDur> time) { return std::chrono::floor<Dur>(time); }
template<typename Dur, typename Rep, typename Ratio> Dur Cast(std::chrono::duration<Rep, Ratio> duration) { return std::chrono::duration_cast<Dur>(duration); }

template<typename Rep, typename Ratio> Ms ToMs(std::chrono::duration<Rep, Ratio> duration) { return Cast<Ms>(duration); }
template<typename Rep, typename Ratio> Secs ToSecs(std::chrono::duration<Rep, Ratio> duration) { return Cast<Secs>(duration); }

auto ToMs(Point time) { return Cast<Ms>(time); }
auto ToMs(LocalPoint time) { return Cast<Ms>(time); }
auto ToSecs(Point time) { return Cast<Secs>(time); }
auto ToSecs(LocalPoint time) { return Cast<Secs>(time); }
auto ToDays(Point time) { return Cast<Days>(time); }
auto ToDays(LocalPoint time) { return Cast<Days>(time); }

Point FromFileTime(FilePoint fileTime)
{
    try { return std::chrono::utc_clock::to_sys(FileClock::to_utc(fileTime)); }
    catch (...) { return Point { fileTime.time_since_epoch() - ToSecs(FileDuration { 0x19DB1DED53E8000LL }) }; }
}

Timestamp ToTimestamp(Point time) { return Clock::to_time_t(time); }
Timestamp ToTimestamp(FilePoint fileTime) { return ToTimestamp(FromFileTime(fileTime)); }
PointSecs FromTimestamp(Timestamp time) { return ToSecs(Clock::from_time_t(time)); }

TimestampMs ToTimestampMs(Point time) { return ToMs(time.time_since_epoch()).count(); }
Point FromTimestampMs(TimestampMs ms) { return Point { Ms { ms } }; }

auto GetCurrentZone() { static auto zone = std::chrono::current_zone(); return zone; }
LocalPoint ToLocal(Point time)
{
    try { return GetCurrentZone()->to_local(time); }
    catch (...)
    {
        auto const timestamp = ToTimestamp(time);
        tm tm { };
        localtime_s(&tm, &timestamp);
        return std::chrono::local_days(std::chrono::year(tm.tm_year + 1900) / (tm.tm_mon + 1) / tm.tm_mday) + Hours(tm.tm_hour) + Mins(tm.tm_min) + Secs(tm.tm_sec);
    }
}
Point FromLocal(LocalPoint time)
{
    try { return GetCurrentZone()->to_sys(time); }
    catch (...)
    {
        auto const days = ToDays(time);
        std::chrono::year_month_day ymd { days };
        std::chrono::hh_mm_ss hms { time - days };
        tm tm
        {
            .tm_sec = (int32)hms.seconds().count(),
            .tm_min = (int32)hms.minutes().count(),
            .tm_hour = (int32)hms.hours().count(),
            .tm_mday = (int32)(uint32)ymd.day(),
            .tm_mon = (int32)(uint32)ymd.month() - 1,
            .tm_year = (int32)ymd.year() - 1900,
            .tm_wday = 0,
            .tm_yday = 0,
            .tm_isdst = -1,
        };
        return FromTimestamp(std::mktime(&tm)) + (time - ToSecs(time));
    }
}
LocalPoint LocalNow() { return ToLocal(Now()); }

template<typename Dur, typename From, typename To> Dur Between(From from, To to) { return Cast<Dur>(to - from); }
Duration Between(Point from, Point to) { return Between<Duration>(from, to); }
Secs BetweenSecs(Point from, Point to) { return Between<Secs>(from, to); }

template<typename Dur, typename From> Dur UntilNow(From from) { return Between<Dur>(from, Now()); }
Duration UntilNow(Point from) { return UntilNow<Duration>(from); }
Secs UntilNowSecs(Point from) { return UntilNow<Secs>(from); }

PrecisePoint FrameStart = PreciseNow();
PreciseDuration Delta;
float DeltaSecs = 0.0f;
void UpdateFrameTime()
{
    auto const now = PreciseNow();
    Delta = Between<Us>(std::exchange(FrameStart, now), now);
    DeltaSecs = std::max(1z, Cast<Ms>(Delta).count()) / 1000.0f;
    // TODO: Detect that timezone changed from cached and show a notification suggesting to restart the application
}

struct Fuzzy
{
    std::optional<uint32> Year, Month, Day, Hour, Minute, Second;
    LocalPoint LocalNow = Time::LocalNow();

    bool IsValid() const
    {
        return (Year || Month || Day || Hour)
            && (!Year   || *Year   >= 1900 && *Year   <= 3000)
            && (!Month  || *Month  >=    1 && *Month  <=   12)
            && (!Day    || *Day    >=    1 && *Day    <=   31)
            && (!Hour   || *Hour   >=    0 && *Hour   <=   24)
            && (!Minute || *Minute >=    0 && *Minute <=   59)
            && (!Second || *Second >=    0 && *Second <=   59);
    }
    bool HasTime() const { return Hour || Minute || Second; }

    Fuzzy GetNext() const
    {
        auto next = *this;
             if (next.Second) ++*next.Second;
        else if (next.Minute) ++*next.Minute;
        else if (next.Hour  ) ++*next.Hour;
        else if (next.Day   ) ++*next.Day;
        else if (next.Month ) ++*next.Month;
        else if (next.Year  ) ++*next.Year;
        return next;
    }

    LocalPoint GetStartLocal() const
    {
        std::chrono::year_month_day const nowDate { ToDays(LocalNow) };
        auto const hasTime = HasTime();
        auto const date = std::chrono::year(
            Year .value_or(hasTime || Day || Month ? ( int32)nowDate.year () : 0)) /
            Month.value_or(hasTime || Day          ? (uint32)nowDate.month() : 1) /
            Day  .value_or(hasTime                 ? (uint32)nowDate.day  () : 1);
        return std::chrono::local_days(date) + Hours(Hour.value_or(0)) + Mins(Minute.value_or(0)) + Secs(Second.value_or(0));
    }
    Point GetStart() const { return FromLocal(GetStartLocal()); }

    LocalPoint GetEndLocal() const { return GetNext().GetStartLocal() - Duration(1); }
    Point GetEnd() const { return GetNext().GetStart() - Duration(1); }

    std::string Format(LocalPoint local) const
    {
        std::string_view formatDate;
        if (HasTime() || Day)
            formatDate = "{0:%B %d, %Y}";
        else if (Month)
            formatDate = "{0:%B of %Y}";
        else if (Year)
            formatDate = "year {0:%Y}";
        else
            formatDate = "today's";

        std::string_view formatTime;
        if (Second)
            formatTime = "{0:%H:%M:%S}";
        else if (Minute)
            formatTime = "{0:%H:%M}";
        else if (Hour)
            formatTime = "hour {0:%H}";

        auto const secs = ToSecs(local);
        return std::vformat(std::format("{}{}{}", formatDate, formatTime.empty() ? "" : " ", formatTime), std::make_format_args(secs));
    }
    std::string FormatStart() const { return Format(GetStartLocal()); }
    std::string FormatEnd() const { return Format(GetEndLocal()); }

    friend bool operator==(LocalPoint local, Fuzzy const& fuzzy)
    {
        //return local >= fuzzy.GetStartLocal() && local <= fuzzy.GetEndLocal();
        auto const datePoint = ToDays(local);
        std::chrono::year_month_day const date { datePoint };
        std::chrono::hh_mm_ss const time { local - datePoint };
        std::chrono::year_month_day const nowDate { ToDays(fuzzy.LocalNow) };
        auto const hasTime = fuzzy.HasTime();
        return (!fuzzy.Year  && !(hasTime || fuzzy.Day || fuzzy.Month) || fuzzy.Year .value_or(( int32)nowDate.year ()) == ( int32)date.year ())
            && (!fuzzy.Month && !(hasTime || fuzzy.Day)                || fuzzy.Month.value_or((uint32)nowDate.month()) == (uint32)date.month())
            && (!fuzzy.Day   && ! hasTime                              || fuzzy.Day  .value_or((uint32)nowDate.day  ()) == (uint32)date.day  ())
            && (!fuzzy.Hour   || fuzzy.Hour   == (uint32)time.hours  ().count())
            && (!fuzzy.Minute || fuzzy.Minute == (uint32)time.minutes().count())
            && (!fuzzy.Second || fuzzy.Second == (uint32)time.seconds().count());
    }
    friend bool operator!=(LocalPoint local, Fuzzy const& fuzzy) { return !(local == fuzzy);}
    friend bool operator< (LocalPoint local, Fuzzy const& fuzzy) { return local <  fuzzy.GetStartLocal(); }
    friend bool operator<=(LocalPoint local, Fuzzy const& fuzzy) { return local <= fuzzy.GetEndLocal(); }
    friend bool operator> (LocalPoint local, Fuzzy const& fuzzy) { return local >  fuzzy.GetEndLocal(); }
    friend bool operator>=(LocalPoint local, Fuzzy const& fuzzy) { return local >= fuzzy.GetStartLocal(); }

    friend bool operator==(Point time, Fuzzy const& fuzzy) { return ToLocal(time) == fuzzy; }
    friend bool operator!=(Point time, Fuzzy const& fuzzy) { return ToLocal(time) != fuzzy; }
    friend bool operator< (Point time, Fuzzy const& fuzzy) { return ToLocal(time) <  fuzzy; }
    friend bool operator<=(Point time, Fuzzy const& fuzzy) { return ToLocal(time) <= fuzzy; }
    friend bool operator> (Point time, Fuzzy const& fuzzy) { return ToLocal(time) >  fuzzy; }
    friend bool operator>=(Point time, Fuzzy const& fuzzy) { return ToLocal(time) >= fuzzy; }
};

}

using namespace GW2Viewer;

template<>
struct scn::scanner<Time::Duration> : empty_parser
{
    template<typename Context>
    error scan(Time::Duration& out, Context& ctx)
    {
        using Utils::Scan::TryScan;
        auto matched = false;
        Time::Duration duration = { };
        while (true)
        {
            uint32 value;
            if (TryScan(ctx, "{}ms", value))
                duration += Time::Ms(value);
            else if (TryScan(ctx, "{}mo", value))
                duration += Time::Months(value);
            else if (TryScan(ctx, "{}s", value))
                duration += Time::Secs(value);
            else if (TryScan(ctx, "{}m", value))
                duration += Time::Mins(value);
            else if (TryScan(ctx, "{}h", value))
                duration += Time::Hours(value);
            else if (TryScan(ctx, "{}d", value))
                duration += Time::Days(value);
            else if (TryScan(ctx, "{}y", value))
                duration += Time::Years(value);
            else
                break;
            matched = true;
        }

        if (!matched)
            return error(error::invalid_scanned_value, "Invalid duration format");

        out = duration;
        return { };
    }
};

template<>
struct scn::scanner<Time::Fuzzy> : empty_parser
{
    template<typename Context>
    error scan(Time::Fuzzy& out, Context& ctx)
    {
        using Utils::Scan::TryScan;
        uint32 year, month, day, hour, minute, second;
        Time::Fuzzy fuzzy;
        if (TryScan(ctx, "{}:{}:{}", hour, minute, second))
            std::tie(fuzzy.Hour, fuzzy.Minute, fuzzy.Second) = std::tie(hour, minute, second);
        else if (TryScan(ctx, "{}:{}:", hour, minute) || TryScan(ctx, "{}:{}", hour, minute))
            std::tie(fuzzy.Hour, fuzzy.Minute) = std::tie(hour, minute);
        else if (TryScan(ctx, "{}:", hour))
            std::tie(fuzzy.Hour) = std::tie(hour);
        scn::skip_range_whitespace(ctx, false);

        if (!ctx.range().empty() && *ctx.range().begin() != '+')
        {
            if (TryScan(ctx, "{}-{}-{}", year, month, day))
                std::tie(fuzzy.Year, fuzzy.Month, fuzzy.Day) = std::tie(year, month, day);
            else if (TryScan(ctx, "{}-{}-", month, day) || TryScan(ctx, "{}-{}", month, day))
                if (month > 12)
                    std::tie(fuzzy.Year, fuzzy.Month) = std::tie(month, day);
                else
                    std::tie(fuzzy.Month, fuzzy.Day) = std::tie(month, day);
            else if (TryScan(ctx, "{}-", day) || TryScan(ctx, "{}", day))
                if (day > 31)
                    std::tie(fuzzy.Year) = std::tie(day);
                else
                    std::tie(fuzzy.Day) = std::tie(day);
            scn::skip_range_whitespace(ctx, false);

            if (!fuzzy.HasTime() && !ctx.range().empty() && *ctx.range().begin() != '+')
            {
                if (TryScan(ctx, "{}:{}:{}", hour, minute, second))
                    std::tie(fuzzy.Hour, fuzzy.Minute, fuzzy.Second) = std::tie(hour, minute, second);
                else if (TryScan(ctx, "{}:{}:", hour, minute) || TryScan(ctx, "{}:{}", hour, minute))
                    std::tie(fuzzy.Hour, fuzzy.Minute) = std::tie(hour, minute);
                else if (TryScan(ctx, "{}:", hour) || TryScan(ctx, "{}", hour))
                    std::tie(fuzzy.Hour) = std::tie(hour);
                scn::skip_range_whitespace(ctx, false);
            }
        }

        if (!fuzzy.IsValid())
            return error(error::invalid_scanned_value, "No values scanned");

        out = fuzzy;
        return { };
    }
};
