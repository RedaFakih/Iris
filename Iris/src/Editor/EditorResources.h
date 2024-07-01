#pragma once

#include "Renderer/Texture.h"

#include <filesystem>

namespace Iris {

	class EditorResources
	{
	public:
		inline static Ref<Texture2D> IrisLogo = nullptr;

		inline static Ref<Texture2D> AssetIcon = nullptr;
		inline static Ref<Texture2D> CameraIcon = nullptr;
		inline static Ref<Texture2D> StaticMeshIcon = nullptr;
		inline static Ref<Texture2D> SkyLightIcon = nullptr;
		inline static Ref<Texture2D> DirectionalLightIcon = nullptr;
		inline static Ref<Texture2D> TextIcon = nullptr;
		inline static Ref<Texture2D> SpriteIcon = nullptr;
		inline static Ref<Texture2D> EyeIcon = nullptr;
		inline static Ref<Texture2D> ClosedEyeIcon = nullptr;
		inline static Ref<Texture2D> TransformIcon = nullptr;

		inline static Ref<Texture2D> LitMaterialIcon = nullptr;
		inline static Ref<Texture2D> UnLitMaterialIcon = nullptr;
		inline static Ref<Texture2D> WireframeViewIcon = nullptr;

		inline static Ref<Texture2D> MinimizeIcon = nullptr;
		inline static Ref<Texture2D> MaximizeIcon = nullptr;
		inline static Ref<Texture2D> RestoreIcon = nullptr;
		inline static Ref<Texture2D> CloseIcon = nullptr;

		inline static Ref<Texture2D> SearchIcon = nullptr;
		inline static Ref<Texture2D> ClearIcon = nullptr;
		inline static Ref<Texture2D> GearIcon = nullptr;
		inline static Ref<Texture2D> PencilIcon = nullptr;
		inline static Ref<Texture2D> PlusIcon = nullptr;
		inline static Ref<Texture2D> PointerIcon = nullptr;
		inline static Ref<Texture2D> TranslateIcon = nullptr;
		inline static Ref<Texture2D> RotateIcon = nullptr;
		inline static Ref<Texture2D> ScaleIcon = nullptr;
		inline static Ref<Texture2D> DropdownIcon = nullptr;
		
		inline static Ref<Texture2D> LeftSquareIcon = nullptr;
		inline static Ref<Texture2D> RightSquareIcon = nullptr;
		inline static Ref<Texture2D> TopSquareIcon = nullptr;
		inline static Ref<Texture2D> BottomSquareIcon = nullptr;
		inline static Ref<Texture2D> BackSquareIcon = nullptr;
		inline static Ref<Texture2D> FrontSquareIcon = nullptr;
		inline static Ref<Texture2D> PerspectiveIcon = nullptr;

		inline static Ref<Texture2D> CheckerboardTexture = nullptr;

		// Content Browser
		inline static Ref<Texture2D> DirectoryIcon = nullptr;
		inline static Ref<Texture2D> FileIcon = nullptr;

		static void Init()
		{
			TextureSpecification spec;
			spec.WrapMode = TextureWrap::Clamp;

			IrisLogo = LoadTexture("IrisIcon.png", "IrisLogo", spec);
			
			AssetIcon = LoadTexture("Icons/Generic.png", "AssetIcon", spec);
			CameraIcon = LoadTexture("Icons/Camera.png", "CameraIcon", spec);
			StaticMeshIcon = LoadTexture("Icons/StaticMesh.png", "StaticMeshIcon", spec);
			SkyLightIcon = LoadTexture("Icons/SkyLight.png", "SkyLightIcon", spec);
			DirectionalLightIcon = LoadTexture("Icons/DirectionalLight.png", "DirectionalLightIcon", spec);
			TextIcon = LoadTexture("Icons/Text.png", "TextIcon", spec);
			SpriteIcon = LoadTexture("Icons/SpriteRenderer.png", "SpriteIcon", spec);
			TransformIcon = LoadTexture("Icons/Transform.png", "TransformIcon", spec);

			MinimizeIcon = LoadTexture("Viewport/Minimize.png", "MinimizeIcon", spec);
			MaximizeIcon = LoadTexture("Viewport/Maximize.png", "MaximizeIcon", spec);
			RestoreIcon = LoadTexture("Viewport/Restore.png", "MaximizeIcon", spec);
			CloseIcon = LoadTexture("Viewport/Close.png", "CloseIcon", spec);

			EyeIcon = LoadTexture("Generic/Eye.png", "EyeIcon", spec);
			ClosedEyeIcon = LoadTexture("Generic/ClosedEye.png", "ClosedEyeIcon", spec);
			LitMaterialIcon = LoadTexture("Generic/LitMaterial.png", "LitMaterialIcon", spec);
			UnLitMaterialIcon = LoadTexture("Generic/UnlitMaterial.png", "UnLitMaterialIcon", spec);
			WireframeViewIcon = LoadTexture("Generic/WireframeView.png", "WireframeViewIcon", spec);

			SearchIcon = LoadTexture("Generic/Search.png", "SearchIcon", spec);
			ClearIcon = LoadTexture("Generic/Clear.png", "ClearIcon", spec);
			GearIcon = LoadTexture("Generic/Gear.png", "GearIcon", spec);
			PencilIcon = LoadTexture("Generic/Pencil.png", "PencilIcon", spec);
			PlusIcon = LoadTexture("Generic/Plus.png", "PlusIcon", spec);
			PointerIcon = LoadTexture("Generic/Pointer.png", "PointerIcon", spec);
			TranslateIcon = LoadTexture("Generic/MoveTool.png", "TranslateIcon", spec);
			RotateIcon = LoadTexture("Generic/RotateTool.png", "RotateIcon", spec);
			ScaleIcon = LoadTexture("Generic/ScaleTool.png", "ScaleIcon", spec);
			DropdownIcon = LoadTexture("Generic/Dropdown.png", "DropdownIcon", spec);

			LeftSquareIcon = LoadTexture("Generic/LeftSquare.png", "LeftSquareIcon", spec);
			RightSquareIcon = LoadTexture("Generic/RightSquare.png", "RightSquareIcon", spec);
			TopSquareIcon = LoadTexture("Generic/TopSquare.png", "TopSquareIcon", spec);
			BottomSquareIcon = LoadTexture("Generic/BottomSquare.png", "BottomSquareIcon", spec);
			BackSquareIcon = LoadTexture("Generic/BackSquare.png", "BackSquare", spec);
			FrontSquareIcon = LoadTexture("Generic/FrontSquare.png", "FrontSquare", spec);
			PerspectiveIcon = LoadTexture("Generic/Perspective.png", "PerspectiveIcon", spec);

			CheckerboardTexture = LoadTexture("Generic/Checkerboard.png", spec);

			DirectoryIcon = LoadTexture("ContentBrowser/DirectoryIcon.png", "DirectoryIcon", spec);
			FileIcon = LoadTexture("ContentBrowser/FileIcon.png", "FileIcon", spec);
		}

		static void Shutdown()
		{
			IrisLogo.Reset();
			
			AssetIcon.Reset();
			CameraIcon.Reset();
			StaticMeshIcon.Reset();
			SkyLightIcon.Reset();
			DirectionalLightIcon.Reset();
			TextIcon.Reset();
			SpriteIcon.Reset();
			TransformIcon.Reset();
			EyeIcon.Reset();
			ClosedEyeIcon.Reset();

			LitMaterialIcon.Reset();
			UnLitMaterialIcon.Reset();
			WireframeViewIcon.Reset();

			MinimizeIcon.Reset();
			MaximizeIcon.Reset();
			RestoreIcon.Reset();
			CloseIcon.Reset();

			SearchIcon.Reset();
			ClearIcon.Reset();
			GearIcon.Reset();
			PencilIcon.Reset();
			PlusIcon.Reset();
			PointerIcon.Reset();
			TranslateIcon.Reset();
			RotateIcon.Reset();
			ScaleIcon.Reset();
			DropdownIcon.Reset();

			LeftSquareIcon.Reset();
			RightSquareIcon.Reset();
			TopSquareIcon.Reset();
			BottomSquareIcon.Reset();
			BackSquareIcon.Reset();
			FrontSquareIcon.Reset();
			PerspectiveIcon.Reset();

			CheckerboardTexture.Reset();

			DirectoryIcon.Reset();
			FileIcon.Reset();
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