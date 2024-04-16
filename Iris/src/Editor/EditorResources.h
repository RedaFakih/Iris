#pragma once

#include "Renderer/Texture.h"

#include <filesystem>

namespace Iris {

	class EditorResources
	{
	public:
		inline static Ref<Texture2D> SearchIcon = nullptr;
		inline static Ref<Texture2D> ClearIcon = nullptr;

		static void Init()
		{
			TextureSpecification spec;
			spec.WrapMode = TextureWrap::Clamp;

			SearchIcon = LoadTexture("Generic/Search.png", "SearchIcon", spec);
			ClearIcon = LoadTexture("Generic/Clear.png", "ClearIcon", spec);
		}

		static void Shutdown()
		{
			SearchIcon.Reset();
			ClearIcon.Reset();
		}

	private:
		static Ref<Texture2D> LoadTexture(const std::filesystem::path& relativePath, TextureSpecification specification = TextureSpecification())
		{
			return LoadTexture(relativePath, "", specification);
		}

		static Ref<Texture2D> LoadTexture(const std::filesystem::path& relativePath, const std::string& name, TextureSpecification specification)
		{
			specification.DebugName = name;

			std::filesystem::path path = std::filesystem::path("Resources") / "Editor" / relativePath;

			if (!std::filesystem::exists(path))
			{
				IR_ERROR_TAG("Editor", "Failed to load editor resource icon {0}! The file doesn't exist.", path.string());
				IR_ASSERT(false);
				return nullptr;
			}

			return Texture2D::Create(specification, path.string());
		}

	};

}