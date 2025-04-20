#pragma once

namespace Iris{

	struct EditorSettings
	{
		/// ContentBrowser
		int ContentBrowserThumbnailSize = 128;

		/// General Editor
		bool HighlighUnsetMeshes = true;
		bool HighlightExtraDynamicLights = true;
		float TranslationSnapValue = 1.0f;
		float RotationSnapValue = 45.0f;
		float ScaleSnapValue = 1.0f;

		static EditorSettings& Get();
	};

	struct EditorSettingsSerializer
	{
		static void Init();

		static void Deserialize();
		static void Serialize();
	};

}