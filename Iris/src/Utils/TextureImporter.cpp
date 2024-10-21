#include "IrisPCH.h"
#include "TextureImporter.h"

#include <stb_image/stb_image.h>

namespace Iris::Utils {

    Buffer TextureImporter::LoadImageFromFile(const std::string& filename, ImageFormat& outFormat, uint32_t& imageWidth, uint32_t& imageHeight)
	{
		Buffer result;
        int width = 0, height = 0, channels = 0;
        if (stbi_is_hdr(filename.c_str()))
        {
            result.Data = reinterpret_cast<uint8_t*>(stbi_loadf(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha));
            result.Size = (uint64_t)width * (uint64_t)height * 4u * sizeof(float);
            outFormat = ImageFormat::RGBA32F;
        }
        else
        {
            result.Data = static_cast<uint8_t*>(stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha));
            result.Size = (uint64_t)width * (uint64_t)height * 4u * sizeof(stbi_uc);
            outFormat = ImageFormat::RGBA;
        }

        if (!result.Data)
            return {};

        imageWidth = static_cast<uint32_t>(width);
        imageHeight = static_cast<uint32_t>(height);

		return result;
	}

	Buffer TextureImporter::LoadImageFromMemory(Buffer buffer, ImageFormat& outFormat, uint32_t& imageWidth, uint32_t& imageHeight)
	{
        Buffer result;
        int width = 0, height = 0, channels = 0;

        if (stbi_is_hdr_from_memory(static_cast<const stbi_uc*>(buffer.Data), (int)buffer.Size))
        {
            result.Data = reinterpret_cast<uint8_t*>(stbi_loadf_from_memory(static_cast<const stbi_uc*>(buffer.Data), (int)buffer.Size, &width, &height, &channels, STBI_rgb_alpha));
            result.Size = (uint64_t)width * (uint64_t)height * 4u * sizeof(float);
            outFormat = ImageFormat::RGBA32F;
        }
        else
        {
            result.Data = static_cast<uint8_t*>(stbi_load_from_memory(static_cast<const stbi_uc*>(buffer.Data), (int)buffer.Size, &width, &height, &channels, STBI_rgb_alpha));
            result.Size = (uint64_t)width * (uint64_t)height * 4u * sizeof(stbi_uc);
            outFormat = ImageFormat::RGBA;
        }

        if (!result.Data)
            return {};

        imageWidth = static_cast<uint32_t>(width);
        imageHeight = static_cast<uint32_t>(height);

        return result;
	}

    void TextureImporter::FreeImageMemory(uint8_t* data)
    {
        stbi_image_free(reinterpret_cast<void*>(data));
    }

}