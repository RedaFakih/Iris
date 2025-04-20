#include "IrisPCH.h"
#include "EditorSettings.h"

#include "Utils/FileSystem.h"

#include <yaml-cpp/yaml.h>

namespace Iris {

	static std::filesystem::path s_EditorSettingsPath;

	void EditorSettingsSerializer::Init()
	{
		s_EditorSettingsPath = std::filesystem::absolute("Config");

		if (!FileSystem::Exists(s_EditorSettingsPath))
			FileSystem::CreateDirectory(s_EditorSettingsPath);

		s_EditorSettingsPath /= "EditorSettings.yaml";

		Deserialize();
	}

	void EditorSettingsSerializer::Deserialize()
	{
		// Generate default settings file if one doesn't exist
		if (!FileSystem::Exists(s_EditorSettingsPath))
		{
			Serialize();
			return;
		}

		std::ifstream stream(s_EditorSettingsPath);
		IR_VERIFY(stream);
		std::stringstream ss;
		ss << stream.rdbuf();
		stream.close();

		EditorSettings& editorSettings = EditorSettings::Get();

		try {
			YAML::Node data = YAML::Load(ss.str());

			YAML::Node rootNode = data["EditorSettings"];
			if (!rootNode)
				return;

			YAML::Node editorGeneralNode = rootNode["EditorGeneral"];
			if (editorGeneralNode)
			{
				editorSettings.HighlighUnsetMeshes = editorGeneralNode["HighlighUnsetMeshes"].as<bool>(true);
				editorSettings.HighlightExtraDynamicLights = editorGeneralNode["HighlightExtraDynamicLights"].as<bool>(true);
				editorSettings.TranslationSnapValue = editorGeneralNode["TranslationSnapValue"].as<float>(1.0f);
				editorSettings.RotationSnapValue = editorGeneralNode["RotationSnapValue"].as<float>(45.0f);
				editorSettings.ScaleSnapValue = editorGeneralNode["ScaleSnapValue"].as<float>(1.0f);
			}

			YAML::Node contentBrowserNode = rootNode["ContentBrowser"];
			if (contentBrowserNode)
			{
				editorSettings.ContentBrowserThumbnailSize = editorGeneralNode["ContentBrowserThumbnailSize"].as<int>(128);
			}
		}
		catch ([[maybe_unused]] const YAML::Exception& e) {
			IR_CORE_ERROR("Could not deserialize EditorSettings!");
			return;
		}
	}

	void EditorSettingsSerializer::Serialize()
	{
		const EditorSettings& editorSettings = EditorSettings::Get();

		YAML::Emitter out;

		out << YAML::BeginMap;

		// EditorSettings
		out << YAML::Key << "EditorSettings" << YAML::Value << YAML::BeginMap;

		// EditorGeneral
		out << YAML::Key << "EditorGeneral" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "HighlighUnsetMeshes" << YAML::Value << editorSettings.HighlighUnsetMeshes;
		out << YAML::Key << "HighlightExtraDynamicLights" << YAML::Value << editorSettings.HighlightExtraDynamicLights;
		out << YAML::Key << "TranslationSnapValue" << YAML::Value << editorSettings.TranslationSnapValue;
		out << YAML::Key << "RotationSnapValue" << YAML::Value << editorSettings.RotationSnapValue;
		out << YAML::Key << "ScaleSnapValue" << YAML::Value << editorSettings.ScaleSnapValue;
		out << YAML::EndMap;
		// EditorGeneral

		// ContentBrowser
		out << YAML::Key << "ContentBrowser" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "ContentBrowserThumbnailSize" << YAML::Value << editorSettings.ContentBrowserThumbnailSize;
		out << YAML::EndMap;
		// ContentBrowser

		out << YAML::EndMap;
		// EditorSettings

		out << YAML::EndMap;

		std::ofstream fout(s_EditorSettingsPath);
		fout << out.c_str();
		fout.close();
	}

	EditorSettings& EditorSettings::Get()
	{
		static EditorSettings settings;
		return settings;
	}

}