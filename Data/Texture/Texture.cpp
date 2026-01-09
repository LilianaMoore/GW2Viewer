module;
#include <d3d11.h>

module GW2Viewer.Data.Texture;

namespace GW2Viewer::Data::Texture
{

Texture::~Texture()
{
    if (Handle.GetTexID())
        ((ID3D11ShaderResourceView*)Handle.GetTexID())->Release();
}

void TextureEntry::UpdateUnloadTime()
{
    if (!UnloadTimeout.count() && Texture)
    {
        auto const pixels = Texture->Width * Texture->Height;
        if (pixels <= 16 * 16)
            UnloadTimeout = 1h;
        else if (pixels <= 128 * 128)
            UnloadTimeout = 10min;
        else
            UnloadTimeout = 10s;
    }
    if (UnloadTimeout.count())
        UnloadTime = Time::FrameStart + UnloadTimeout;
}

}
