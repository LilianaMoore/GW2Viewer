export module GW2Viewer.Common;
import std;

export namespace GW2Viewer
{

using namespace std::string_view_literals;

using byte = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using sbyte = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;

enum class Language : byte
{
    English,
    Korean,
    French,
    German,
    Spanish,
    Chinese,
};

enum class Race
{
    Asura,
    Charr,
    Human,
    Norn,
    Sylvari,
    Max
};

enum class Sex
{
    Male,
    Female,
    None,
    Max
};

struct Radians;
struct Degrees
{
    Degrees() { }
    Degrees(float degrees) : m_degrees(degrees) { }
    Degrees(Radians const& radians);

    float GetDegrees() const { return m_degrees; }
    float GetRadians() const { return m_degrees / 180.0f * std::numbers::pi_v<float>; }
    float GetCos() const { return std::cos(GetRadians()); }
    float GetSin() const { return std::sin(GetRadians()); }

    operator bool() const { return m_degrees != 0.0f; }
    operator float() const { return m_degrees; }

private:
    float m_degrees = 0.0f;
};
struct Radians
{
    Radians() { }
    Radians(float radians) : m_radians(radians) { }
    Radians(Degrees const& degrees) : Radians(degrees.GetRadians()) { }

    float GetDegrees() const { return m_radians * 180.0f / std::numbers::pi_v<float>; }
    float GetRadians() const { return m_radians; }
    float GetCos() const { return std::cos(GetRadians()); }
    float GetSin() const { return std::sin(GetRadians()); }

    operator bool() const { return m_radians != 0.0f; }
    operator float() const { return m_radians; }

private:
    float m_radians = 0.0f;
};
Degrees::Degrees(Radians const& radians) : m_degrees(radians.GetDegrees()) { }

}
