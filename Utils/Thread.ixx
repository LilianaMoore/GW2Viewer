module;
#include <Windows.h>

export module GW2Viewer.Utils.Thread;
import GW2Viewer.Common.Time;
import GW2Viewer.Utils.Encoding;
import std;

export namespace GW2Viewer::Utils::Thread
{

void SetName(std::wstring_view name)
{
    SetThreadDescription(GetCurrentThread(), name.data());
}
void SetName(std::string_view name)
{
    SetName(Encoding::FromUTF8(name));
}

template<typename Func, typename... Args>
auto Async(std::wstring name, Func&& func, Args&&... args)
{
    return std::async(std::launch::async, [name, call = std::bind(std::forward<Func>(func), std::forward<Args>(args)...)] mutable
    {
        SetName(name);
        return std::invoke(call);
    });
}
template<typename Func, typename... Args>
auto Async(std::string_view name, Func&& func, Args&&... args)
{
    return Async(Encoding::FromUTF8(name), std::forward<Func>(func), std::forward<Args>(args)...);
}

void Sleep(Time::Duration period)
{
    std::this_thread::sleep_for(period);
}
void SleepWhile(Time::Duration period, auto&& pred)
{
    while (std::invoke(pred))
        std::this_thread::sleep_for(period);
}
void SleepUntil(Time::Duration period, auto&& pred)
{
    while (!std::invoke(pred))
        std::this_thread::sleep_for(period);
}

}
