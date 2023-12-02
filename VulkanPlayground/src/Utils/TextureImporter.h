#pragma once

#include "Core/Buffer.h"
#include "Renderer/Texture.h"

namespace vkPlayground::Utils {

	class TextureImporter
	{
	public:
		static Buffer LoadImageFromFile(const std::string& filename, ImageFormat& outFormat, uint32_t& imageWidth, uint32_t& imageHeight);
		static Buffer LoadImageFromMemory(Buffer buffer, ImageFormat& outFormat, uint32_t& imageWidth, uint32_t& imageHeight);

		// NOTE: This exists since we load the images with stb which uses malloc and our Buffer class uses delete[] to clear memory.
		// this malloc/delete mismatch could lead to UB
		static void FreeImageMemory(const uint8_t* data);
	};

}