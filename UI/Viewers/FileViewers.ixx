export module GW2Viewer.UI.Viewers.FileViewers;
import GW2Viewer.Common;
import GW2Viewer.Common.FourCC;
import GW2Viewer.Data.Archive;
import GW2Viewer.UI.Viewers.FileViewer;
import GW2Viewer.UI.Viewers.PackFileViewer;
import std;

export namespace GW2Viewer::UI::Viewers
{

struct FileViewers
{
    using ConstructorFunction = std::unique_ptr<FileViewer>(*)(uint32 id, bool newTab, Data::Archive::File const& file);

    struct RegisteredViewer
    {
        fcc FourCC;
        uint32 FourCCMask;
        ConstructorFunction Constructor;
    };

    static auto& GetRegistry()
    {
        static std::list<RegisteredViewer> instance { };
        return instance;
    }

    template<fcc FourCC, uint32 FourCCMask = 0xFFFFFFFF>
    struct For;
    template<fcc FourCC, uint32 FourCCMask>
    struct For : FileViewer { };

    template<fcc FourCC, uint32 FourCCMask = 0xFFFFFFFF>
    class Register
    {
        static bool DoRegister()
        {
            GetRegistry().emplace_back(FourCC, FourCCMask, [](uint32 id, bool newTab, Data::Archive::File const& file) -> std::unique_ptr<FileViewer> { return std::make_unique<For<FourCC, FourCCMask>>(id, newTab, file); });
            return true;
        }
        inline static bool m_registered = DoRegister();
    };
};

template<> struct FileViewers::For<fcc::PF, 0xFFFF> : Register<fcc::PF, 0xFFFF>, PackFileViewer { using PackFileViewer::PackFileViewer; };

}
