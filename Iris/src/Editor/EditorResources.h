#pragma once

#include "Renderer/Texture.h"

#include <filesystem>

namespace Iris {

	class EditorResources
	{
	public:
		inline static Ref<Texture2D> IrisLogo = nullptr;

		// Content Browser
		inline static Ref<Texture2D> DirectoryIcon = nullptr;
		inline static Ref<Texture2D> FileIcon = nullptr;

		// General
		inline static Ref<Texture2D> MinimizeIcon = nullptr;
		inline static Ref<Texture2D> MaximizeIcon = nullptr;
		inline static Ref<Texture2D> RestoreIcon = nullptr;
		inline static Ref<Texture2D> CloseIcon = nullptr;
		inline static Ref<Texture2D> SearchIcon = nullptr;
		inline static Ref<Texture2D> ClearIcon = nullptr;
		inline static Ref<Texture2D> CheckerboardTexture = nullptr;
		inline static Ref<Texture2D> PlusIcon = nullptr;
		inline static Ref<Texture2D> EyeDropperIcon = nullptr;

		// Scene Hierarchy
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
		inline static Ref<Texture2D> PencilIcon = nullptr;
		inline static Ref<Texture2D> RigidBodyIcon = nullptr;
		inline static Ref<Texture2D> BoxColliderIcon = nullptr;
		inline static Ref<Texture2D> SphereColliderIcon = nullptr;
		inline static Ref<Texture2D> CylinderColliderIcon = nullptr;
		inline static Ref<Texture2D> CapsuleColliderIcon = nullptr;
		inline static Ref<Texture2D> CompoundColliderIcon = nullptr;
		inline static Ref<Texture2D> MeshColliderIcon = nullptr;
		inline static Ref<Texture2D> RigidBody2DIcon = nullptr;
		inline static Ref<Texture2D> BoxCollider2DIcon = nullptr;
		inline static Ref<Texture2D> CircleCollider2DIcon = nullptr;

		// Viewport
		inline static Ref<Texture2D> LitMaterialIcon = nullptr;
		inline static Ref<Texture2D> UnLitMaterialIcon = nullptr;
		inline static Ref<Texture2D> WireframeViewIcon = nullptr;
		inline static Ref<Texture2D> GearIcon = nullptr;
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
		inline static Ref<Texture2D> ScenePlayIcon = nullptr;
		inline static Ref<Texture2D> SceneStopIcon = nullptr;
		inline static Ref<Texture2D> ScenePauseIcon = nullptr;

		static void Init()
		{
			TextureSpecification spec;
			spec.WrapMode = TextureWrap::Clamp;

			IrisLogo = LoadTexture("IrisIcon.png", "IrisLogo", spec);

			// Content Browser panel
			DirectoryIcon = LoadTexture("Icons/ContentBrowser/DirectoryIcon.png", "DirectoryIcon", spec);
			FileIcon = LoadTexture("Icons/ContentBrowser/FileIcon.png", "FileIcon", spec);

			// General
			MinimizeIcon = LoadTexture("Icons/General/Minimize.png", "MinimizeIcon", spec);
			MaximizeIcon = LoadTexture("Icons/General/Maximize.png", "MaximizeIcon", spec);
			RestoreIcon = LoadTexture("Icons/General/Restore.png", "MaximizeIcon", spec);
			CloseIcon = LoadTexture("Icons/General/Close.png", "CloseIcon", spec);
			SearchIcon = LoadTexture("Icons/General/Search.png", "SearchIcon", spec);
			ClearIcon = LoadTexture("Icons/General/Clear.png", "ClearIcon", spec);
			CheckerboardTexture = LoadTexture("Icons/General/Checkerboard.png", spec);
			PlusIcon = LoadTexture("Icons/General/Plus.png", "PlusIcon", spec);
			EyeDropperIcon = LoadTexture("Icons/General/EyeDropper.png", "EyeDropper", spec);

			// Scene Hierarchy panel
			AssetIcon = LoadTexture("Icons/SceneHierarchyPanel/Generic.png", "AssetIcon", spec);
			CameraIcon = LoadTexture("Icons/SceneHierarchyPanel/Camera.png", "CameraIcon", spec);
			StaticMeshIcon = LoadTexture("Icons/SceneHierarchyPanel/StaticMesh.png", "StaticMeshIcon", spec);
			SkyLightIcon = LoadTexture("Icons/SceneHierarchyPanel/SkyLight.png", "SkyLightIcon", spec);
			DirectionalLightIcon = LoadTexture("Icons/SceneHierarchyPanel/DirectionalLight.png", "DirectionalLightIcon", spec);
			TextIcon = LoadTexture("Icons/SceneHierarchyPanel/Text.png", "TextIcon", spec);
			SpriteIcon = LoadTexture("Icons/SceneHierarchyPanel/SpriteRenderer.png", "SpriteIcon", spec);
			EyeIcon = LoadTexture("Icons/SceneHierarchyPanel/Eye.png", "EyeIcon", spec);
			ClosedEyeIcon = LoadTexture("Icons/SceneHierarchyPanel/ClosedEye.png", "ClosedEyeIcon", spec);
			TransformIcon = LoadTexture("Icons/SceneHierarchyPanel/Transform.png", "TransformIcon", spec);
			PencilIcon = LoadTexture("Icons/SceneHierarchyPanel/Pencil.png", "PencilIcon", spec);
			RigidBodyIcon = LoadTexture("Icons/SceneHierarchyPanel/RigidBody.png", "RigidBodyIcon", spec);
			BoxColliderIcon = LoadTexture("Icons/SceneHierarchyPanel/BoxCollider.png", "BoxColliderIcon", spec);
			SphereColliderIcon = LoadTexture("Icons/SceneHierarchyPanel/SphereCollider.png", "SphereColliderIcon", spec);
			// TODO: For now it uses the capsule collider icon, however we need to create a new CylinderCollider icon
			CylinderColliderIcon = LoadTexture("Icons/SceneHierarchyPanel/CapsuleCollider.png", "CapsuleColliderIcon", spec);
			CapsuleColliderIcon = LoadTexture("Icons/SceneHierarchyPanel/CapsuleCollider.png", "CapsuleColliderIcon", spec);
			CompoundColliderIcon = LoadTexture("Icons/SceneHierarchyPanel/CompoundCollider.png", "CompoundColliderIcon", spec);
			MeshColliderIcon = LoadTexture("Icons/SceneHierarchyPanel/MeshCollider.png", "MeshColliderIcon", spec);
			RigidBody2DIcon = LoadTexture("Icons/SceneHierarchyPanel/RigidBody2D.png", "RigidBody2DIcon", spec);
			BoxCollider2DIcon = LoadTexture("Icons/SceneHierarchyPanel/BoxCollider2D.png", "BoxCollider2DIcon", spec);
			CircleCollider2DIcon = LoadTexture("Icons/SceneHierarchyPanel/CircleCollider2D.png", "CicleCollider2DIcon", spec);

			// Viewport
			LitMaterialIcon = LoadTexture("Icons/Viewport/LitMaterial.png", "LitMaterialIcon", spec);
			UnLitMaterialIcon = LoadTexture("Icons/Viewport/UnlitMaterial.png", "UnLitMaterialIcon", spec);
			WireframeViewIcon = LoadTexture("Icons/Viewport/WireframeView.png", "WireframeViewIcon", spec);
			GearIcon = LoadTexture("Icons/Viewport/Gear.png", "GearIcon", spec);
			PointerIcon = LoadTexture("Icons/Viewport/Pointer.png", "PointerIcon", spec);
			TranslateIcon = LoadTexture("Icons/Viewport/MoveTool.png", "TranslateIcon", spec);
			RotateIcon = LoadTexture("Icons/Viewport/RotateTool.png", "RotateIcon", spec);
			ScaleIcon = LoadTexture("Icons/Viewport/ScaleTool.png", "ScaleIcon", spec);
			DropdownIcon = LoadTexture("Icons/Viewport/Dropdown.png", "DropdownIcon", spec);
			LeftSquareIcon = LoadTexture("Icons/Viewport/LeftSquare.png", "LeftSquareIcon", spec);
			RightSquareIcon = LoadTexture("Icons/Viewport/RightSquare.png", "RightSquareIcon", spec);
			TopSquareIcon = LoadTexture("Icons/Viewport/TopSquare.png", "TopSquareIcon", spec);
			BottomSquareIcon = LoadTexture("Icons/Viewport/BottomSquare.png", "BottomSquareIcon", spec);
			BackSquareIcon = LoadTexture("Icons/Viewport/BackSquare.png", "BackSquare", spec);
			FrontSquareIcon = LoadTexture("Icons/Viewport/FrontSquare.png", "FrontSquare", spec);
			PerspectiveIcon = LoadTexture("Icons/Viewport/Perspective.png", "PerspectiveIcon", spec);
			ScenePlayIcon = LoadTexture("Icons/Viewport/Play.png", "PerspectiveIcon", spec);
			SceneStopIcon = LoadTexture("Icons/Viewport/Stop.png", "PerspectiveIcon", spec);
			ScenePauseIcon = LoadTexture("Icons/Viewport/Pause.png", "PerspectiveIcon", spec);
		}

		static void Shutdown()
		{
			IrisLogo.Reset();
			
			// Content Browser
			DirectoryIcon.Reset();
			FileIcon.Reset();

			// General
			MinimizeIcon.Reset();
			MaximizeIcon.Reset();
			RestoreIcon.Reset();
			CloseIcon.Reset();
			SearchIcon.Reset();
			ClearIcon.Reset();
			CheckerboardTexture.Reset();
			PlusIcon.Reset();
			EyeDropperIcon.Reset();

			// Scene Hierarchy
			AssetIcon.Reset();
			CameraIcon.Reset();
			StaticMeshIcon.Reset();
			SkyLightIcon.Reset();
			DirectionalLightIcon.Reset();
			TextIcon.Reset();
			SpriteIcon.Reset();
			EyeIcon.Reset();
			ClosedEyeIcon.Reset();
			TransformIcon.Reset();
			PencilIcon.Reset();
			RigidBodyIcon.Reset();
			BoxColliderIcon.Reset();
			SphereColliderIcon.Reset();
			CylinderColliderIcon.Reset();
			CapsuleColliderIcon.Reset();
			CompoundColliderIcon.Reset();
			MeshColliderIcon.Reset();
			RigidBody2DIcon.Reset();
			BoxCollider2DIcon.Reset();
			CircleCollider2DIcon.Reset();

			// Viewport
			LitMaterialIcon.Reset();
			UnLitMaterialIcon.Reset();
			WireframeViewIcon.Reset();
			GearIcon.Reset();
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
			ScenePlayIcon.Reset();
			SceneStopIcon.Reset();
			ScenePauseIcon.Reset();
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