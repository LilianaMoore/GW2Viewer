export module GW2Viewer.Data.Content:ContentName;
import :Symbols;

export namespace GW2Viewer::Data::Content
{

struct ContentName
{
    Symbols::String<wchar_t>::Struct const* Name;
    Symbols::String<wchar_t>::Struct const* FullName;
};

}
