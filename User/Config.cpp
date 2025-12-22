module GW2Viewer.User.Config;
import GW2Viewer.Common.JSON;
import GW2Viewer.UI.Notifications;

static std::filesystem::path const configPath = R"(.\config.json)";

namespace GW2Viewer::User
{

bool Config::Load()
{
    auto contents = (std::stringstream() << std::ifstream(configPath).rdbuf()).str();
    if (contents.empty())
        return true;
    from_json(json::parse(contents, nullptr, false), *this);
    FinishLoading();
    G::Notifications.AddTimed(1s, {
        .Type = GW2Viewer::UI::Notification::Types::Success,
        .Text = "Config loaded.",
    });
    return true;
}
bool Config::Save()
{
    std::erase_if(ContentNamespaceNames, [](auto const& pair) { return pair.second.empty(); });
    std::erase_if(ContentObjectNames, [](auto const& pair) { return pair.second.empty(); });
    std::ofstream file(configPath);
    if (!file.is_open())
    {
        G::Notifications.AddCloseable({
            .Type = GW2Viewer::UI::Notification::Types::Error,
            .Text = "Config save failed:\nUnable to open the destination file for writing.",
        });
        return false;
    }
    if (!(file << ordered_json(*this).dump(2)))
    {
        G::Notifications.AddCloseable({
            .Type = GW2Viewer::UI::Notification::Types::Error,
            .Text = "Config save failed:\nUnable to write data to the file.",
        });
        return false;
    }
    G::Notifications.AddTimed(1s, {
        .Type = GW2Viewer::UI::Notification::Types::Success,
        .Text = "Config saved.",
    });
    return true;
}

}
