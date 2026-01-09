module GW2Viewer.Data.Pack.Manager;
import GW2Viewer.Utils.ScanPE;
import <cctype>;

namespace GW2Viewer::Data::Pack
{

void Manager::Load(std::filesystem::path const& path, Utils::Async::ProgressBarContext& progress)
{
    progress.Start(std::format("Parsing PackFile layouts from {}", path.filename().string()));
    using namespace Layout;
    Utils::ScanPE::Scanner scanner { path };

    struct PackFileField
    {
        UnderlyingTypes UnderlyingType;
        RealTypes RealType;
        uint32 Unk;
        char const* Name;
        union
        {
            PackFileField* ElementFields;
            PackFileField** VariantElementFields;
            void(*PostProcessStruct)();
        };
        uint16 Size;
    };
    struct PackFileVersion
    {
        PackFileField* Fields;
        void* PostProcessFunction;
        void* Unk;
    };
    auto collect = [&](PackFileField* fields, auto& collect) -> Type const*
    {
        if (!fields)
            return nullptr;
        PackFileField* field = fields;
        while (true)
        {
            if (!scanner.rdata.Valid(field->Name))
                return nullptr;
            if (field->UnderlyingType != UnderlyingTypes::StructDefinition && !scanner.rdata.Valid(field->ElementFields) && !scanner.data.Valid(field->ElementFields))
                return nullptr;
            if (field->UnderlyingType == UnderlyingTypes::StructDefinition)
                return &m_types.try_emplace((byte const*)fields,
                    field->Name,
                    field->Size,
                    std::vector { std::from_range,
                    std::span { fields, field }
                    | std::views::transform([&](PackFileField const& field) -> Field {
                        return {
                            field.Name,
                            field.UnderlyingType,
                            field.RealType,
                            field.Size,
                            field.UnderlyingType != UnderlyingTypes::Variant ? collect(field.ElementFields, collect) : nullptr,
                            field.UnderlyingType == UnderlyingTypes::Variant ? std::vector { std::from_range, std::span { field.VariantElementFields, field.Size } | std::views::transform([&collect](auto* fields) { return collect(fields, collect); }) } : std::vector<Type const*> { },
                        };
                    })
                }).first->second;
            ++field;
        }
    };

    progress.Start(scanner.rdata.size());
    for (auto p = scanner.rdata.begin(); p < scanner.rdata.end(); p += sizeof(void*))
    {
        if (isalnum(p[0]) && isalnum(p[1]) && isalnum(p[2]) && (!p[3] || isalnum(p[3])))
        {
            uint32 const numVersions = *(uint32 const*)&p[4];
            if (!numVersions || numVersions > 100)
                continue;

            if (auto const versions = *(PackFileVersion**)&p[8]; versions && scanner.rdata.Valid(versions))
            {
                for (uint32 versionNum = 0; versionNum < numVersions; ++versionNum)
                {
                    auto& version = versions[versionNum];
                    if (!version.Fields)
                        continue;
                    if (!scanner.rdata.Valid(version.Fields))
                        goto fail;
                    if (!scanner.text.Valid(version.PostProcessFunction))
                        goto fail;

                    if (auto type = collect(version.Fields, collect))
                        m_chunks[std::string((char const*)p, p[3] ? 4 : 3)].try_emplace(versionNum, type);
                    else
                        goto fail;
                }
            }

        fail:;
        }

        if (auto const offset = std::distance(scanner.rdata.begin(), p); !(offset % (100 * sizeof(void*))))
            progress = offset;
    }

    if (m_chunks["AFNT"].empty())
    {
        auto const filename = m_types.try_emplace(new byte(), Type
        {
            .Name = "<Font File>",
            .DeclaredSize = 6,
            .Fields =
            {
                { .Name = "File", .UnderlyingType = UnderlyingTypes::FileName },
            },
        }).first;
        auto const font = m_types.try_emplace(new byte(), Type
        {
            .Name = "<Font>",
            .DeclaredSize = 68,
            .Fields =
            {
                { .Name = "Language", .UnderlyingType = UnderlyingTypes::Byte },
                { .Name = "Scale", .UnderlyingType = UnderlyingTypes::Byte },
                { .Name = "Token", .UnderlyingType = UnderlyingTypes::Qword, .RealType = RealTypes::Token },
                { .Name = "Flags", .UnderlyingType = UnderlyingTypes::Dword },
                { .Name = "ExtentX", .UnderlyingType = UnderlyingTypes::Byte },
                { .Name = "ExtentY", .UnderlyingType = UnderlyingTypes::Byte },
                { .Name = "Files", .UnderlyingType = UnderlyingTypes::InlineArray, .ArraySize = 13, .ElementType = &filename->second },
            },
        }).first;
        auto const fonts = m_types.try_emplace(new byte(), Type
        {
            .Name = "<Fonts>",
            .DeclaredSize = 8,
            .Fields =
            {
                { .Name = "Fonts", .UnderlyingType = UnderlyingTypes::DwordArray, .ElementType = &font->second },
            },
        }).first;
        m_chunks["AFNT"].try_emplace(0, &fonts->second);
    }

    m_loaded = true;
}

}
