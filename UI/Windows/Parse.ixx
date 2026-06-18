export module GW2Viewer.UI.Windows.Parse;
import GW2Viewer.Common;
import GW2Viewer.Common.GUID;
import GW2Viewer.Common.Token32;
import GW2Viewer.Common.Token64;
import GW2Viewer.Data.Content.Mangling;
import GW2Viewer.Data.Game;
import GW2Viewer.Data.Pack;
import GW2Viewer.Data.Text.Format;
import GW2Viewer.Data.Text.Hash;
import GW2Viewer.UI.Controls;
import GW2Viewer.UI.ImGui;
import GW2Viewer.UI.Windows.Window;
import GW2Viewer.Utils.Base64;
import GW2Viewer.Utils.ConstString;
import GW2Viewer.Utils.Encoding;
import GW2Viewer.Utils.Scan;
import GW2Viewer.Utils.Visitor;
import std;
#include "Macros.h"

export namespace GW2Viewer::UI::Windows
{

struct Parse : Window
{
    std::string Title() override { return "Parse"; }
    void Draw() override
    {
        if (scoped::WithStyleVar(ImGuiStyleVar_CellPadding, ImVec2()))
        if (scoped::Table("Conversions", 3, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Hideable))
        {
            I::TableSetupColumn("Left");
            I::TableSetupColumn("Direction", ImGuiTableColumnFlags_WidthFixed);
            I::TableSetupColumn("Right");
            I::TableSetColumnEnabled(2, !IsVerticalLayout());

            Conversion<"uint64", "hex", uint64, hex<uint64>>({
                .ConvertLR = [](uint64 const& in, hex<uint64>& out) { out = in; },
                .ConvertRL = [](hex<uint64> const& in, uint64& out) { out = in; },
            });
            Conversion<"ImGui 0xAABBGGRR", "GW2 0xAARRGGBB", hex<uint32>, hex<uint32>>({
                .ConvertLR = [](hex<uint32> const& in, hex<uint32>& out) { out = IM_COL32(in.Value >> 16 & 0xFF, in.Value >> 8 & 0xFF, in.Value & 0xFF, in.Value >> 24 & 0xFF); },
                .ConvertRL = [](hex<uint32> const& in, hex<uint32>& out) { out = IM_COL32(in.Value >> 16 & 0xFF, in.Value >> 8 & 0xFF, in.Value & 0xFF, in.Value >> 24 & 0xFF); },
                .DrawR = [](hex<uint32> const& hex)
                {
                    auto color = I::ColorConvertU32ToFloat4(hex);
                    std::swap(color.x, color.z);
                    I::SetNextItemWidth(-FLT_MIN);
                    I::ColorEdit4("##Color", &color.x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
                },
            });
            Conversion<"uint64", "Token32", uint32, Token32>({
                .ConvertLR = [](uint32 const& in, Token32& out) { out = *(Token32*)&in; },
                .ConvertRL = [](Token32 const& in, uint32& out) { out = *(uint32*)&in; },
            });
            Conversion<"uint64", "Token64", uint64, Token64>({
                .ConvertLR = [](uint64 const& in, Token64& out) { out = *(Token64*)&in; },
                .ConvertRL = [](Token64 const& in, uint64& out) { out = *(uint64*)&in; },
            });
            Conversion<"Filename", "File ID", uint64, Data::Pack::FileReference>({
                .ConvertLR = [](uint64 const& in, Data::Pack::FileReference& out) { out = *(Data::Pack::FileReference*)&in; },
                .ConvertRL = [](Data::Pack::FileReference const& in, uint64& out) { out = 0; memcpy(&out, &in, sizeof(in)); },
                .DrawR = [](Data::Pack::FileReference const& file) { Controls::FileButton(file.GetFileID()); },
            });

            Separator();
            Conversion<"hex[16]", "GUID", std::array<byte, 16>, GUID>({
                .ConvertLR = [](std::array<byte, 16> const& in, GUID& out) { out = *(GUID const*)in.data(); },
                .ConvertRL = [](GUID const& in, std::array<byte, 16>& out) { out = *(std::array<byte, 16> const*)&in; },
                .DrawR = [](GUID const& guid) { Controls::ContentButton(G::Game.Content.GetByGUID(guid), nullptr); },
            });
            Conversion<"uint32[4]", "GUID", std::array<uint32, 4>, GUID>({
                .ConvertLR = [](std::array<uint32, 4> const& in, GUID& out) { out = *(GUID const*)in.data(); },
                .ConvertRL = [](GUID const& in, std::array<uint32, 4>& out) { out = *(std::array<uint32, 4> const*)&in; },
                .DrawR = [](GUID const& guid) { Controls::ContentButton(G::Game.Content.GetByGUID(guid), nullptr); },
            });
            Conversion<"uint64[2]", "GUID", std::array<uint64, 2>, GUID>({
                .ConvertLR = [](std::array<uint64, 2> const& in, GUID& out) { out = *(GUID const*)in.data(); },
                .ConvertRL = [](GUID const& in, std::array<uint64, 2>& out) { out = *(std::array<uint64, 2> const*)&in; },
                .DrawR = [](GUID const& guid) { Controls::ContentButton(G::Game.Content.GetByGUID(guid), nullptr); },
            });

            Separator();
            Conversion<"wstring", "Mangled Content Name", std::wstring, std::wstring>({
                .ConvertLR = [](std::wstring const& in, std::wstring& out) { out = Data::Content::MangleFullName(in); },
            });
            Conversion<"wstring", "Format String", std::wstring, std::wstring>({
                .ConvertLR = [](std::wstring const& in, std::wstring& out) { out = Data::Text::FormatStringEmpty(in); },
            });
            Conversion<"hex[]", "Base64", std::vector<byte>, std::string>({
                .ConvertLR = [](std::vector<byte> const& in, std::string& out) { out = Utils::Base64::Encode({ (char const*)in.data(), in.size() }); },
                .ConvertRL = [](std::string const& in, std::vector<byte>& out) { out.assign_range(Utils::Base64::Decode(in)); },
            });

            Separator();
            Conversion<"string", "IStrHash<char>", std::string, hex<uint32>>({
                .ConvertLR = [](std::string const& in, hex<uint32>& out) { out = Data::Text::IStrHash(in); },
                .ConvertRL = [](hex<uint32> const& in, std::string& out) { BruteforceStringHash.Run<char, &Data::Text::IStrHash>(in.Value, out); },
            });
            Conversion<"string", "IStrHashI<char>", std::string, hex<uint32>>({
                .ConvertLR = [](std::string const& in, hex<uint32>& out) { out = Data::Text::IStrHashI(in); },
                .ConvertRL = [](hex<uint32> const& in, std::string& out) { BruteforceStringHash.Run<char, &Data::Text::IStrHashI>(in.Value, out); },
            });
            Conversion<"wstring", "IStrHash<wchar_t>", std::wstring, hex<uint32>>({
                .ConvertLR = [](std::wstring const& in, hex<uint32>& out) { out = Data::Text::IStrHash(in); },
                .ConvertRL = [](hex<uint32> const& in, std::wstring& out) { BruteforceStringHash.Run<wchar_t, &Data::Text::IStrHash>(in.Value, out); },
            });
            Conversion<"wstring", "IStrHashI<wchar_t>", std::wstring, hex<uint32>>({
                .ConvertLR = [](std::wstring const& in, hex<uint32>& out) { out = Data::Text::IStrHashI(in); },
                .ConvertRL = [](hex<uint32> const& in, std::wstring& out) { BruteforceStringHash.Run<wchar_t, &Data::Text::IStrHashI>(in.Value, out); },
                .DrawL = [](std::wstring const&) { I::AlignTextToFramePadding(); I::RightAlignedText("<c=#4>Bruteforce alphabet:</c>"); },
                .DrawR = [](hex<uint32> const&) { I::SetNextItemWidth(-FLT_MIN); I::InputText("##Bruteforce", &BruteforceStringHash.Alphabet); },
            });
        }
    }

    template<typename L, typename R>
    struct ConversionOptions
    {
        void(*ConvertLR)(L const& in, R& out) = nullptr;
        void(*ConvertRL)(R const& in, L& out) = nullptr;
        void(*DrawL)(L const& l) = nullptr;
        void(*DrawR)(R const& r) = nullptr;
    };

    static void Separator()
    {
        I::Separator();
    }

    template<ConstString LabelLeft, ConstString LabelRight, typename L, typename R>
    static void Conversion(ConversionOptions<L, R> const& options = {})
    {
        //static constexpr auto success = [](auto const& result) { return result && result.empty(); };

        static auto parse = Utils::Visitor::Overloaded
        {
            [](std::string_view in, std::string& out)
            {
                out = in;
                return true;
            },
            [](std::string_view in, std::wstring& out)
            {
                out = Utils::Encoding::FromUTF8(in);
                return true;
            },
            [](std::string_view in, GUID& out)
            {
                return (bool)Utils::Scan::Into(in, "{}", out);
            },
            [](std::string_view in, Token32& out)
            {
                out = in;
                return true;
            },
            [](std::string_view in, Token64& out)
            {
                out = in;
                return true;
            },
            [](std::string_view in, Data::Pack::FileReference& out)
            {
                if (uint32 fileID; Utils::Scan::Into(in, "{}", fileID))
                {
                    auto* chars = (uint16*)&out;
                    chars[0] = 0xFF + fileID % 0xFF00;
                    chars[1] = 0x100 + fileID / 0xFF00;
                    chars[2] = 0;
                    return true;
                }
                return false;
            },
            [](std::string_view in, std::array<byte, 16>& out)
            {
                out.fill(0);
                for (auto&& [i, o] : std::views::zip(in | std::views::split(" "sv), out))
                    if (!Utils::Scan::NumberLiteral(std::string_view(i), "{:x}", o))
                        return false;
                return true;
            },
            [](std::string_view in, std::array<uint32, 4>& out)
            {
                out.fill(0);
                for (auto&& [i, o] : std::views::zip(in | std::views::split(" "sv), out))
                    if (!Utils::Scan::NumberLiteral(std::string_view(i), "{}", o) && !Utils::Scan::NumberLiteral(std::string_view(i), "{:x}", o))
                        return false;
                return true;
            },
            [](std::string_view in, std::array<uint64, 2>& out)
            {
                out.fill(0);
                for (auto&& [i, o] : std::views::zip(in | std::views::split(" "sv), out))
                    if (!Utils::Scan::NumberLiteral(std::string_view(i), "{}", o) && !Utils::Scan::NumberLiteral(std::string_view(i), "{:x}", o))
                        return false;
                return true;
            },
            [](std::string_view in, std::vector<byte>& out)
            {
                out.clear();
                for (auto&& i : in | std::views::split(" "sv))
                    if (byte o; Utils::Scan::NumberLiteral(std::string_view(i), "{:x}", o))
                        out.emplace_back(o);
                return true;
            },
            []<std::integral T>(std::string_view in, T& out)
            {
                /* TODO
                if (in.contains(' '))
                {
                    auto type = [&]<typename BufferT>(BufferT)
                    {
                        std::vector<BufferT> tokens;
                        BufferT buffer;
                        auto range = std::ranges::subrange(in);
                        out = { };
                        size_t i = 0;
                        while (auto result = Utils::Scan::Into(range, "{:02x}", buffer))
                        {
                            out |= (T)buffer << i++ * 8 * sizeof(BufferT);
                            range = result.Rest();
                            //if (result.empty() || std::ranges::all_of(range, isspace))
                            //    return true;
                        }
                        return false;
                    };
                    if (type(byte())) return true;
                    if (type(uint16())) return true;
                }
                */
                if (Utils::Scan::NumberLiteral(in, "{}", out))
                    return true;
                if (Utils::Scan::NumberLiteral(in, "{:x}", out))
                    return true;
                return false;
            },
            []<std::integral T>(std::string_view in, hex<T>& out) { return Utils::Scan::NumberLiteral(in, "{:x}", out.Value); },
            [](std::string_view in, auto& out) { return false; }
        };
        static auto print = Utils::Visitor::Overloaded
        {
            []<std::integral T>(T const& in) { return std::format("{}", in); },
            []<std::integral T>(hex<T> const& in) { return std::format("{:X}", in.Value); },
            [](GUID const& in) { return std::format("{}", in); },
            [](Token32 const& in) { return std::format("{}", in.GetString().data()); },
            [](Token64 const& in) { return std::format("{}", in.GetString().data()); },
            [](Data::Pack::FileReference const& in) { return std::format("{}", in.GetFileID()); },
            [](std::array<byte, 16> const& in) { return std::format("{:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}", in[0], in[1], in[2], in[3], in[4], in[5], in[6], in[7], in[8], in[9], in[10], in[11], in[12], in[13], in[14], in[15]); },
            [](std::array<uint32, 4> const& in) { return std::format("0x{:X} 0x{:X} 0x{:X} 0x{:X}", in[0], in[1], in[2], in[3]); },
            [](std::array<uint64, 2> const& in) { return std::format("0x{:X} 0x{:X}", in[0], in[1]); },
            [](std::vector<byte> const& in) { return std::format("{:s}", std::views::join_with(in | std::views::transform([](byte b) { return std::format("{:02X}", b); }), " "sv)); },
            [](std::string_view const& in) { return in; },
            [](std::wstring_view const& in) { return Utils::Encoding::ToUTF8(in); },
        };
        static std::string inputL, inputR;
        static L l { };
        static R r { };
        static bool successL = false, successR = false;

        bool const vertical = IsVerticalLayout();

        if (scoped::WithID(LabelLeft.str))
        if (scoped::WithID(LabelRight.str))
        {
            I::TableNextRow();
            I::TableNextColumn();
            I::SetNextItemWidth(-FLT_MIN);
            if (scoped::WithColorVar(ImGuiCol_Text, I::GetColorU32(ImGuiCol_Text, successL ? 1.0f : 0.5f)))
            if (I::InputTextWithHint("##L", LabelLeft.str, &inputL) && (successL = true, successR = parse(inputL, l)))
            {
                options.ConvertLR(l, r);
                inputR = print(r);
            }
            I::SetItemTooltip(LabelLeft.str);

            I::TableNextColumn();
            I::AlignTextToFramePadding();
            I::TextUnformatted(options.ConvertLR && options.ConvertRL ? ICON_FA_ARROWS_LEFT_RIGHT : options.ConvertLR ? ICON_FA_RIGHT_LONG : options.ConvertRL ? ICON_FA_LEFT_LONG : "");

            if (vertical)
                I::TableNextRow();

            I::TableNextColumn();
            I::SetNextItemWidth(-FLT_MIN);
            if (scoped::WithColorVar(ImGuiCol_Text, I::GetColorU32(ImGuiCol_Text, successR ? 1.0f : 0.5f)))
            if (I::InputTextWithHint("##R", LabelRight.str, &inputR, options.ConvertRL ? 0 : ImGuiInputTextFlags_ReadOnly) && (successR = true, successL = parse(inputR, r)) && options.ConvertRL)
            {
                options.ConvertRL(r, l);
                inputL = print(l);
            }
            I::SetItemTooltip(LabelRight.str);

            if (options.DrawL || options.DrawR)
            {
                I::TableNextRow();
                I::TableNextColumn(); if (options.DrawL) options.DrawL(l);
                I::TableNextColumn();
                if (vertical)
                    I::TableNextRow();
                I::TableNextColumn(); if (options.DrawR) options.DrawR(r);
            }

            if (vertical)
                I::Separator();
        }
    }

private:
    template<typename T>
    struct hex
    {
        T Value;

        hex() : Value() { }
        hex(T const& value) : Value(value) { }
        operator T() const { return Value; }
        hex& operator=(T const& value) { Value = value; return *this; }
    };

    static bool IsVerticalLayout() { auto const size = I::GetWindowSize(); return size.y > size.x && size.x <= 400; }

    static inline struct
    {
        std::string Alphabet = "abcdefghijklmnopqrstuvwxyz\"-:";
        uint32 MaxLength = 6;

        template<typename Ch, uint32(&Hasher)(std::basic_string_view<Ch>)>
        void Run(uint32 hash, std::basic_string<Ch>& out)
        {
            out.clear();
            auto const alphabet = Alphabet;
            auto const maxLength = MaxLength;
            std::mutex mutex;
            std::for_each(std::execution::par_unseq, Alphabet.begin(), Alphabet.end(), [&, hash, maxLength](Ch firstChar)
            {
                std::array<Ch, 32> str { };
                str[0] = firstChar;
                auto bruteforce = [&, hash, maxLength](int i, char c, auto& bruteforce) -> void
                {
                    str[i] = c;
                    if (std::basic_string_view<Ch> view(str.data(), i + 1); Hasher(view) == hash)
                    {
                        std::scoped_lock _(mutex);
                        if (!out.empty())
                            out += ConstString(" OR ").get<Ch>();
                        out += view;
                    }

                    if (i + 1 < MaxLength)
                        for (auto const next : Alphabet)
                            bruteforce(i + 1, next, bruteforce);
                };
                bruteforce(0, firstChar, bruteforce);
            });
        }
    } BruteforceStringHash;
};

}

export namespace GW2Viewer::G::Windows { UI::Windows::Parse Parse; }
